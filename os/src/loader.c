#include <timeros/os.h>
#include <timeros/fs.h>

static u8 flags_to_mmap_prot(u32 flags)
{
    return (flags & PF_R ? PTE_R : 0) |
           (flags & PF_W ? PTE_W : 0) |
           (flags & PF_X ? PTE_X : 0);
}

static int read_exact(efs_u32 id, u64 offset, void *dst, size_t len)
{
    if (offset > 0xffffffffU) return -1;
    return efs_read_at(fs_instance(), id, (efs_u32)offset, dst, len) == (efs_ssize)len ? 0 : -1;
}

static int program_header(efs_u32 id, const elf64_ehdr_t *ehdr,
                          u16 index, elf64_phdr_t *phdr)
{
    return read_exact(id, ehdr->e_phoff + (u64)index * sizeof(*phdr), phdr, sizeof(*phdr));
}

int load_elf(unsigned id, ProcessControlBlock *proc, u64 *entry)
{
    EfsDiskInode file;
    elf64_ehdr_t ehdr;
    if (efs_inode(fs_instance(), id, &file) < 0 || file.type != EFS_FILE ||
        file.size < sizeof(ehdr) || read_exact(id, 0, &ehdr, sizeof(ehdr)) < 0) return -1;
    if (*(u32 *)ehdr.e_ident != ELFMAG || ehdr.e_ident[EI_CLASS] != ELFCLASS64 ||
        ehdr.e_ident[5] != 1 || ehdr.e_machine != EM_RISCV ||
        ehdr.e_phentsize != sizeof(elf64_phdr_t) || ehdr.e_phnum == 0 || ehdr.e_phnum > 64 ||
        ehdr.e_phoff > file.size ||
        (u64)ehdr.e_phnum * ehdr.e_phentsize > file.size - ehdr.e_phoff) return -1;

    u64 max_end = 0;
    for (u16 i = 0; i < ehdr.e_phnum; ++i) {
        elf64_phdr_t ph;
        if (program_header(id, &ehdr, i, &ph) < 0) return -1;
        if (ph.p_type != PT_LOAD) continue;
        if (ph.p_filesz > ph.p_memsz || ph.p_offset > file.size ||
            ph.p_filesz > file.size - ph.p_offset ||
            ph.p_vaddr + ph.p_memsz < ph.p_vaddr ||
            ph.p_vaddr + ph.p_memsz >= TRAMPOLINE ||
            (ph.p_flags & PF_W && !(ph.p_flags & PF_R))) return -1;
        if (ph.p_vaddr + ph.p_memsz > max_end) max_end = ph.p_vaddr + ph.p_memsz;
    }
    u64 stack_limit = THREAD_TRAPFRAME(MAX_THREADS - 1) -
                      (2 * MAX_THREADS + 2) * PAGE_SIZE;
    if (!max_end || max_end > stack_limit - PAGE_SIZE) return -1;
    proc->base_size = PGROUNDUP(max_end);
    proc->ustack_base = proc->base_size;

    for (u16 i = 0; i < ehdr.e_phnum; ++i) {
        elf64_phdr_t ph;
        if (program_header(id, &ehdr, i, &ph) < 0) return -1;
        if (ph.p_type != PT_LOAD || !ph.p_memsz) continue;
        u64 segment_end = ph.p_vaddr + ph.p_memsz;
        u64 file_end = ph.p_vaddr + ph.p_filesz;
        u8 perm = PTE_U | flags_to_mmap_prot(ph.p_flags);
        for (u64 va = PGROUNDDOWN(ph.p_vaddr); va < PGROUNDUP(segment_end); va += PAGE_SIZE) {
            PageTableEntry *pte = find_pte(&proc->pagetable,
                                            floor_virts(virt_addr_from_size_t(va)));
            u64 pa;
            if (!pte) {
                PhysPageNum page = kalloc();
                if (!page.value) return -1;
                pa = phys_addr_from_phys_page_num(page).value;
                if (PageTable_try_map(&proc->pagetable, va, pa, perm) < 0) {
                    kfree(page);
                    return -1;
                }
            } else {
                if (!(pte->bits & PTE_U)) return -1;
                pa = PTE2PA(pte->bits);
                pte->bits |= perm;
            }
            u64 from = va > ph.p_vaddr ? va : ph.p_vaddr;
            u64 to = va + PAGE_SIZE < file_end ? va + PAGE_SIZE : file_end;
            if (from < to && read_exact(id, ph.p_offset + from - ph.p_vaddr,
                                        (void *)(pa + from - va), to - from) < 0) return -1;
        }
    }
    PageTableEntry *start = find_pte(&proc->pagetable,
        floor_virts(virt_addr_from_size_t(ehdr.e_entry)));
    if (!start || (start->bits & (PTE_U | PTE_X)) != (PTE_U | PTE_X)) return -1;
    *entry = ehdr.e_entry;
    return 0;
}

void load_initproc(void)
{
    efs_u32 id;
    if (efs_lookup(fs_instance(), "initproc", &id) < 0) panic("initproc missing from easy-fs");
    ProcessControlBlock *proc = allocproc();
    if (!proc || load_elf(id, proc, &proc->entry) < 0) panic("cannot load initproc ELF");
}
