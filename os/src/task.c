#include <timeros/os.h>
#include <timeros/task.h>
static ProcessControlBlock processes[MAX_TASKS];
static TaskControlBlock tasks[MAX_THREADS];
static int current_slot = -1;
static int nextpid;
static TaskContext boot_context;
extern PageTable kernel_pagetable;
extern u64 kernel_satp;
extern char trampoline[];

static TaskContext tcx_init(u64 kstack_top)
{
    TaskContext cx;
    memset(&cx, 0, sizeof(cx));
    cx.ra = (reg_t)trap_return;
    cx.sp = kstack_top;
    return cx;
}

void procinit(void)
{
    memset(processes, 0, sizeof(processes));
    memset(tasks, 0, sizeof(tasks));
    current_slot = -1;
    nextpid = 0;
}

static int new_pagetable(PageTable *pt)
{
    pt->root_ppn = kalloc();
    if (!pt->root_ppn.value) return -1;
    if (PageTable_try_map(pt, TRAMPOLINE, (u64)trampoline, PTE_R | PTE_X) < 0) {
        kfree(pt->root_ppn);
        pt->root_ppn.value = 0;
        return -1;
    }
    return 0;
}

ProcessControlBlock *allocproc(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        ProcessControlBlock *p = &processes[i];
        if (p->used) continue;
        memset(p, 0, sizeof(*p));
        if (new_pagetable(&p->pagetable) < 0) return 0;
        p->pid = nextpid++;
        p->used = true;
        return p;
    }
    return 0;
}

static int alloc_tid(ProcessControlBlock *p)
{
    for (int i = 0; i < MAX_THREADS; i++)
        if (!p->tid_used[i]) { p->tid_used[i] = true; return i; }
    return -1;
}

static int user_executable(ProcessControlBlock *p, u64 va)
{
    if (!va || (va & 1) || va >= TRAMPOLINE - MAX_THREADS * PAGE_SIZE)
        return 0;
    PageTableEntry *pte = find_pte(&p->pagetable, floor_virts(virt_addr_from_size_t(va)));
    return pte && (pte->bits & (PTE_U | PTE_X)) == (PTE_U | PTE_X);
}

static TaskControlBlock *make_thread(ProcessControlBlock *p, u64 entry, u64 arg, u64 return_stub)
{
    // 分配 TID 和 TCB
    int tid = alloc_tid(p);
    if (tid < 0) return 0;
    TaskControlBlock *t = 0;
    for (int i = 0; i < MAX_THREADS; i++)
        if (tasks[i].task_state == UnInit) { t = &tasks[i]; break; }
    if (!t) { p->tid_used[tid] = false; return 0; }
    memset(t, 0, sizeof(*t));
    t->tid = tid;
    t->process = p;

    t->trap_cx_va = THREAD_TRAPFRAME(tid);
    if (p->ustack_base > THREAD_TRAPFRAME(MAX_THREADS - 1) -
                          (2 * MAX_THREADS + 2) * PAGE_SIZE) {
        memset(t, 0, sizeof(*t));
        p->tid_used[tid] = false;
        return 0;
    }
    u64 stack_bottom = p->ustack_base + ((u64)tid * 2 + 1) * PAGE_SIZE;
    t->ustack = stack_bottom + PAGE_SIZE;
    if (t->ustack <= stack_bottom || t->ustack >= t->trap_cx_va - PAGE_SIZE ||
        find_pte(&p->pagetable, floor_virts(virt_addr_from_size_t(stack_bottom))) ||
        find_pte(&p->pagetable, floor_virts(virt_addr_from_size_t(t->trap_cx_va))))
        goto fail;
    PhysPageNum page = kalloc();
    if (!page.value) goto fail;
    t->trap_cx_paddr = phys_addr_from_phys_page_num(page).value;
    page = kalloc();
    if (!page.value) goto fail;
    u64 user_stack_pa = phys_addr_from_phys_page_num(page).value;
    page = kalloc();
    if (!page.value) { kfree(floor_phys(phys_addr_from_size_t(user_stack_pa))); goto fail; }
    t->kstack_paddr = phys_addr_from_phys_page_num(page).value;
    u64 kstack_bottom = KSTACK((int)(t - tasks));
    t->kstack = kstack_bottom + PAGE_SIZE;
    if (PageTable_try_map(&p->pagetable, t->trap_cx_va, t->trap_cx_paddr, PTE_R | PTE_W) < 0) {
        kfree(floor_phys(phys_addr_from_size_t(user_stack_pa)));
        goto fail;
    }
    if (PageTable_try_map(&p->pagetable, stack_bottom, user_stack_pa, PTE_R | PTE_W | PTE_U) < 0) {
        kfree(floor_phys(phys_addr_from_size_t(user_stack_pa)));
        goto fail;
    }
    if (PageTable_try_map(&kernel_pagetable, kstack_bottom, t->kstack_paddr, PTE_R | PTE_W) < 0)
        goto fail;
    sfence_vma();
    for (int i = 0; i < MAX_THREADS; i++) {
        TaskControlBlock *other = &tasks[i];
        if (other == t || other->process != p || other->task_state == UnInit) continue;
        assert(other->trap_cx_va != t->trap_cx_va);
        assert(other->trap_cx_paddr != t->trap_cx_paddr);
        assert(other->ustack != t->ustack);
        assert(other->kstack != t->kstack);
    }
    TrapContext *cx = (TrapContext *)t->trap_cx_paddr;
    memset(cx, 0, sizeof(*cx));
    cx->sstatus = (r_sstatus() & ~SSTATUS_SPP) | SSTATUS_SPIE;
    cx->sepc = entry;
    cx->sp = t->ustack;
    cx->ra = return_stub;
    cx->a0 = arg;
    cx->kernel_satp = kernel_satp;
    cx->kernel_sp = t->kstack;
    cx->trap_handler = (u64)trap_handler;
    t->task_context = tcx_init(t->kstack);
    t->task_state = Ready;
    p->live_threads++;
    return t;
fail:
    if (t->kstack_paddr) kfree(floor_phys(phys_addr_from_size_t(t->kstack_paddr)));
    if (t->trap_cx_paddr) {
        uvmunmap(&p->pagetable, floor_virts(virt_addr_from_size_t(t->trap_cx_va)), 1, 0);
        kfree(floor_phys(phys_addr_from_size_t(t->trap_cx_paddr)));
    }
    uvmunmap(&p->pagetable, floor_virts(virt_addr_from_size_t(stack_bottom)), 1, 1);
    memset(t, 0, sizeof(*t));
    p->tid_used[tid] = false;
    return 0;
}

void app_init(size_t app_id)
{
    ProcessControlBlock *p = &processes[app_id];
    if (!p->used || !make_thread(p, p->entry, 0, 0)) panic("app_init: no thread");
}

TaskControlBlock *current_thread(void)
{
    return current_slot >= 0 ? &tasks[current_slot] : 0;
}

ProcessControlBlock *current_proc(void)
{
    TaskControlBlock *t = current_thread();
    return t ? t->process : 0;
}

u64 get_current_trap_cx(void) { return current_thread()->trap_cx_paddr; }
u64 current_thread_user_token(void) { return MAKE_SATP(current_proc()->pagetable.root_ppn.value); }
int allocpid(void) { return nextpid++; }

void schedule(void)
{
    TaskControlBlock *old = current_thread();
    // 如果当前线程是运行状态，则将其状态设置为就绪状态
    if (old && old->task_state == Running)
        old->task_state = Ready;

    int start = current_slot < 0 ? 0 : (current_slot + 1) % MAX_THREADS;
    
    for (int step = 0; step < MAX_THREADS; step++) {
        int next = (start + step) % MAX_THREADS;
        if (tasks[next].task_state != Ready) continue;
        if (next == current_slot) { tasks[next].task_state = Running; return; }
        TaskContext *oldcx = old ? &old->task_context : &boot_context;
        tasks[next].task_state = Running;
        current_slot = next;
        __switch(oldcx, &tasks[next].task_context);
        return;
    }
    panic("schedule: no runnable thread");
}

void run_first_task(void)
{
    schedule();
    panic("run_first_task returned");
}

static void reap_thread(TaskControlBlock *t)
{
    ProcessControlBlock *p = t->process;
    if (!p || t == current_thread()) panic("reap running thread");
    u64 stack_bottom = t->ustack - PAGE_SIZE;
    uvmunmap(&p->pagetable, floor_virts(virt_addr_from_size_t(stack_bottom)), 1, 1);
    uvmunmap(&p->pagetable, floor_virts(virt_addr_from_size_t(t->trap_cx_va)), 1, 1);
    uvmunmap(&kernel_pagetable, floor_virts(virt_addr_from_size_t(t->kstack - PAGE_SIZE)), 1, 1);
    sfence_vma();
    p->tid_used[t->tid] = false;
    memset(t, 0, sizeof(*t));
}

void children_proc_clear(ProcessControlBlock *p)
{
    for (int i = 0; i < MAX_TASKS; i++)
        if (processes[i].used && processes[i].parent == p)
            processes[i].parent = &processes[0];
}

void freeproc(ProcessControlBlock *p)
{
    if (!p || !p->used || p == current_proc()) panic("freeproc: active");
    for (int i = 0; i < MAX_THREADS; i++)
        if (tasks[i].process == p) reap_thread(&tasks[i]);
    proc_freepagetable(&p->pagetable, p->base_size);
    memset(p, 0, sizeof(*p));
}

static int sole_main_thread(ProcessControlBlock *p)
{
    if (current_thread()->tid != 0 || p->live_threads != 1) return 0;
    for (int i = 0; i < MAX_THREADS; i++)
        if (tasks[i].process == p && &tasks[i] != current_thread()) return 0;
    return 1;
}

int __sys_fork(void)
{
    ProcessControlBlock *parent = current_proc();
    TaskControlBlock *old = current_thread();
    if (!sole_main_thread(parent)) return -1;
    ProcessControlBlock *child = allocproc();
    if (!child) return -1;
    child->base_size = parent->base_size;
    child->ustack_base = parent->ustack_base;
    child->entry = parent->entry;
    if (uvmcopy(&parent->pagetable, &child->pagetable, child->base_size) < 0) {
        freeproc(child);
        return -1;
    }
    TaskControlBlock *newt = make_thread(child, child->entry, 0, 0);
    if (!newt) { freeproc(child); return -1; }
    PageTableEntry *src = find_pte(&parent->pagetable,
        floor_virts(virt_addr_from_size_t(old->ustack - PAGE_SIZE)));
    PageTableEntry *dst = find_pte(&child->pagetable,
        floor_virts(virt_addr_from_size_t(newt->ustack - PAGE_SIZE)));
    memcpy((void *)PTE2PA(dst->bits), (void *)PTE2PA(src->bits), PAGE_SIZE);
    memcpy((void *)newt->trap_cx_paddr, (void *)old->trap_cx_paddr, sizeof(TrapContext));
    ((TrapContext *)newt->trap_cx_paddr)->a0 = 0;
    ((TrapContext *)newt->trap_cx_paddr)->kernel_sp = newt->kstack;
    child->parent = parent;
    return child->pid;
}

int exec(const char *name)
{
    ProcessControlBlock *p = current_proc();
    TaskControlBlock *t = current_thread();
    if (!sole_main_thread(p)) return -1;
    AppMetadata metadata = get_app_data_by_name((char *)name);
    if (metadata.id < 0) return -1;
    elf64_ehdr_t *ehdr = (elf64_ehdr_t *)metadata.start;
    elf_check(ehdr);
    ProcessControlBlock staged = *p;
    if (new_pagetable(&staged.pagetable) < 0) return -1;
    staged.base_size = 0;
    if (load_segment(ehdr, &staged) < 0) {
        proc_freepagetable(&staged.pagetable, staged.base_size);
        return -1;
    }
    u64 new_stack = staged.ustack_base + PAGE_SIZE;
    PhysPageNum stack_page = kalloc();
    if (!stack_page.value || new_stack + PAGE_SIZE >= t->trap_cx_va ||
        PageTable_try_map(&staged.pagetable, new_stack,
            phys_addr_from_phys_page_num(stack_page).value, PTE_R | PTE_W | PTE_U) < 0 ||
        PageTable_try_map(&staged.pagetable, t->trap_cx_va,
            t->trap_cx_paddr, PTE_R | PTE_W) < 0) {
        if (stack_page.value) kfree(stack_page);
        /* 失败路径清除已映射的栈页，然后释放 ELF 页。 */
        uvmunmap(&staged.pagetable, floor_virts(virt_addr_from_size_t(new_stack)), 1, 0);
        proc_freepagetable(&staged.pagetable, staged.base_size);
        return -1;
    }
    PageTable old_pt = p->pagetable;
    u64 old_size = p->base_size;
    u64 old_stack = t->ustack - PAGE_SIZE;
    p->pagetable = staged.pagetable;
    p->base_size = staged.base_size;
    p->ustack_base = staged.ustack_base;
    p->entry = ehdr->e_entry;
    t->ustack = new_stack + PAGE_SIZE;
    TrapContext *cx = (TrapContext *)t->trap_cx_paddr;
    /* 复用 trap 上下文物理页，但不要把旧程序的寄存器带入新程序。 */
    memset(cx, 0, sizeof(*cx));
    cx->sstatus = (r_sstatus() & ~SSTATUS_SPP) | SSTATUS_SPIE;
    cx->sepc = p->entry;
    cx->sp = t->ustack;
    cx->kernel_satp = kernel_satp;
    cx->kernel_sp = t->kstack;
    cx->trap_handler = (u64)trap_handler;
    uvmunmap(&old_pt, floor_virts(virt_addr_from_size_t(old_stack)), 1, 1);
    uvmunmap(&old_pt, floor_virts(virt_addr_from_size_t(t->trap_cx_va)), 1, 0);
    proc_freepagetable(&old_pt, old_size);
    return 0;
}

int thread_create(u64 entry, u64 arg, u64 return_stub)
{
    ProcessControlBlock *p = current_proc();
    if (!user_executable(p, entry) || !user_executable(p, return_stub)) return -1;
    TaskControlBlock *t = make_thread(p, entry, arg, return_stub);
    return t ? t->tid : -1;
}

static void process_became_zombie(ProcessControlBlock *p, int code)
{
    p->zombie = true;
    p->exit_code = code;
    children_proc_clear(p);
}

void thread_exit(int code)
{
    TaskControlBlock *t = current_thread();
    ProcessControlBlock *p = t->process;
    if (p->pid == 0 && p->live_threads == 1) panic("init exiting");
    t->exit_code = code;
    t->task_state = Zombie;
    p->live_threads--;
    if (!p->live_threads) process_became_zombie(p, code);
    schedule();
    panic("thread_exit returned");
}

void exit_current_and_run_next(u64 exit_code)
{
    ProcessControlBlock *p = current_proc();
    if (p->pid == 0) panic("init exiting");
    for (int i = 0; i < MAX_THREADS; i++)
        if (tasks[i].process == p && tasks[i].task_state != Zombie) {
            tasks[i].task_state = Zombie;
            tasks[i].exit_code = (int)exit_code;
        }
    p->live_threads = 0;
    process_became_zombie(p, (int)exit_code);
    schedule();
    panic("process exit returned");
}

int waittid(u64 tid)
{
    ProcessControlBlock *p = current_proc();
    if (tid >= MAX_THREADS || tid == (u64)current_thread()->tid || !p->tid_used[tid])
        return -1;
    for (int i = 0; i < MAX_THREADS; i++) {
        TaskControlBlock *t = &tasks[i];
        if (t->process != p || (u64)t->tid != tid) continue;
        if (t->task_state != Zombie) return -2;
        int code = t->exit_code;
        reap_thread(t);
        return code;
    }
    return -1;
}

int wait(void)
{
    ProcessControlBlock *p = current_proc();
    for (;;) {
        bool havekids = false;
        for (int i = 0; i < MAX_TASKS; i++) {
            ProcessControlBlock *child = &processes[i];
            if (!child->used || child->parent != p) continue;
            havekids = true;
            if (child->zombie) {
                int pid = child->pid;
                freeproc(child);
                return pid;
            }
        }
        if (!havekids) return -1;
        schedule();
    }
}
