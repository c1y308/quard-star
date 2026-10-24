#include <timeros/loader.h>
#include <timeros/task.h>
#include <timeros/address.h>
extern u64 _num_app[];
/* _app_names: 由 build.c 生成的一组以 '\0' 结尾的应用名字符串, 顺序与 app_id 一致 */
extern char _app_names[];
/* 解析后的应用名指针表, 下标即 app_id */
static char* app_names[MAX_TASKS];

// 获取加载的app数量
size_t get_num_app()
{
    return _num_app[0];
}

AppMetadata  get_app_data(size_t app_id)
{
    AppMetadata metadata;

    size_t num_app = get_num_app();

    metadata.start = _num_app[app_id];  // 获取app起始地址

    metadata.size = _num_app[app_id+1] - _num_app[app_id];    // 获取app结束地址  

    metadata.id = app_id;    // 记录 app 索引, get_app_data_by_name/exec 用其判定是否找到

    assert(app_id <= num_app);

    return metadata;
}

/* 解析 _app_names 字符串表, 把每个应用名的指针填入 app_names[]
   表内是连续存放、以 '\0' 分隔的字符串, 例如 "time\0write\0" */
void get_app_names()
{
    size_t app_num = get_num_app();
    char* p = _app_names;

    printk("/**** APPS ****\n");
    for (size_t i = 0; i < app_num; i++)
    {
        if (i >= MAX_TASKS)
        {
            printk("too many apps, only record %d\n", MAX_TASKS);
            break;
        }
        app_names[i] = p;
        printk("%s\n", app_names[i]);
        /* 跳过当前名字及其结尾 '\0', 定位到下一个名字 */
        p += strlen(app_names[i]) + 1;
    }
    printk("**************/\n");
}

/* 按名字查找 app: 命中返回其元数据(id>=0), 未命中返回 id==-1 的元数据 */
AppMetadata get_app_data_by_name(char* path)
{
    AppMetadata metadata;
    metadata.id = -1;
    metadata.start = 0;
    metadata.size = 0;

    size_t app_num = get_num_app();
    for (size_t i = 0; i < app_num; i++)
    {
        if (strcmp(path, app_names[i]) == 0)
        {
            metadata = get_app_data(i + 1);
            printk("find app:%s id:%d\n", path, metadata.id);
            return metadata;
        }
    }
    printk("app not exit!!\n");
    return metadata;
}


static u8 flags_to_mmap_prot(u8 flags)
{
    return (flags & PF_R ? PTE_R : 0) | 
           (flags & PF_W ? PTE_W : 0) |
           (flags & PF_X ? PTE_X : 0);
}

/* 校验 ELF 魔数与架构 */
void elf_check(elf64_ehdr_t *ehdr)
{
    //判断 elf 文件的魔数
    assert(*(u32 *)ehdr == ELFMAG);
    //判断传入文件是否为 riscv64 的
    if (ehdr->e_machine != EM_RISCV || ehdr->e_ident[EI_CLASS] != ELFCLASS64)
    {
        panic("only riscv64 elf file is supported");
    }
}

/* 解析 program header, 把各 PT_LOAD 段装载到 proc 页表, 并计算 ustack/base_size */
int load_segment(elf64_ehdr_t *ehdr, ProcessControlBlock* proc)
{
    elf64_phdr_t *phdr;
    u64 max_end_va = 0;

    // 遍历每个程序段描述符
    for (size_t i = 0; i < ehdr->e_phnum; i++) {
        // 得到当前程序段描述符在 ELF 文件中的地址
        phdr = (elf64_phdr_t*)((u8*)ehdr + ehdr->e_phoff + (u64)ehdr->e_phentsize * i);
        if (phdr->p_type != PT_LOAD)
            continue;
        u64 end = phdr->p_vaddr + phdr->p_memsz;
        if (end < phdr->p_vaddr || phdr->p_filesz > phdr->p_memsz)
            return -1;
        if (end > max_end_va) max_end_va = end;
    }

    u64 stack_limit = THREAD_TRAPFRAME(MAX_THREADS - 1) - (2 * MAX_THREADS + 2) * PAGE_SIZE;
    if (max_end_va > stack_limit - PAGE_SIZE)
        return -1;

    proc->base_size   = PGROUNDUP(max_end_va);
    proc->ustack_base = proc->base_size;

    // 遍历每个程序段描述符
    for (size_t i = 0; i < ehdr->e_phnum; i++)
    {
        // 得到当前程序段描述符在 ELF 文件中的地址
        phdr = (elf64_phdr_t*)((u8*)ehdr + ehdr->e_phoff + (u64)ehdr->e_phentsize * i);
        if (phdr->p_type == PT_LOAD)
        {
            // 在内核虚拟地址空间中加载的起始地址和结束地址
            u64 segment_start = phdr->p_vaddr;
            u64 segment_end   = segment_start + phdr->p_memsz;
            u64 file_end      = segment_start + phdr->p_filesz;
            if (phdr->p_memsz == 0)
                continue;
            
            // 把 ELF 中的段权限 改为 页表权限
            u8 map_perm = PTE_U | flags_to_mmap_prot(phdr->p_flags);
            // 按页加载内存
            u64 first_page = PGROUNDDOWN(segment_start);
            u64 end_page   = PGROUNDUP(segment_end);
            // 从宽范围页开始遍历映射（包括 .bss 页）
            for (u64 page_va = first_page; page_va < end_page; page_va += PAGE_SIZE)
            {
                PageTableEntry* pte = find_pte(&proc->pagetable,
                                               floor_virts(virt_addr_from_size_t(page_va)));
                u64 paddr;
                 // 虚拟页还未映射物理页
                if (pte == NULL)
                {
                    PhysPageNum ppn = kalloc(); // kalloc 已将新页清零
                    if (!ppn.value) return -1;
                    paddr = phys_addr_from_phys_page_num(ppn).value;
                    if (PageTable_try_map(&proc->pagetable, page_va, paddr, map_perm) < 0) {
                        kfree(ppn);
                        return -1;
                    }
                }
                // 虚拟页已经完成了映射
                else
                {
                    assert(pte->bits & PTE_U);
                    // #define PTE2PA(pte) (((pte) >> 10) << 12)
                    paddr = PTE2PA(pte->bits);
                    pte->bits |= map_perm;
                }
                // 当前映射页的结束地址
                u64 page_end = page_va + PAGE_SIZE;
                // 只 copy 页中有数据的地址（取当前页的起始地址与程序段起始地址中的较大值）
                u64 copy_start = page_va > segment_start ? page_va : segment_start;
                // 只 copy 页中有数据的地址（取当前页的结束地址与文件数据结束地址中的较小值）
                u64 copy_end = page_end < file_end ? page_end : file_end;
                
                if (copy_start < copy_end)
                    // (copy_start - page_va) ：数据在当前页面中的偏移
                    memcpy((void*)(paddr + copy_start - page_va),
                           (u8*)ehdr + phdr->p_offset + (copy_start - segment_start),
                           copy_end - copy_start);

                // 清除 .bss 区
                u64 zero_start = page_va > file_end ? page_va : file_end;
                u64 zero_end = page_end < segment_end ? page_end : segment_end;
                if (zero_start < zero_end)
                    memset((void*)(paddr + zero_start - page_va), 0, zero_end - zero_start);
            }
        }
    }

    return 0;
}

void load_app(size_t app_id)
{
    //加载ELF文件
    AppMetadata metadata = get_app_data(app_id + 1);

    //ELF 文件头
    elf64_ehdr_t *ehdr = (elf64_ehdr_t*)metadata.start;

    //校验 ELF 魔数与架构
    elf_check(ehdr);

    // 创建进程及其用户页表
    ProcessControlBlock* proc = allocproc();
    if (!proc) panic("load_app: no process slot or memory");

    //加载程序段
    if (load_segment(ehdr, proc) < 0) panic("load_app: invalid ELF or out of memory");

    //记录APP程序的入口地址，为 main 函数地址
    proc->entry = (u64)ehdr->e_entry;

}
