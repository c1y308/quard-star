#include <timeros/loader.h>
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
void load_segment(elf64_ehdr_t *ehdr, struct TaskControlBlock* proc)
{
    // Program Header 解析
    elf64_phdr_t *phdr;
    //遍历每一个逻辑段
    for (size_t i = 0; i < ehdr->e_phnum; i++)
    {
        //拿到每个Program Header的指针
        phdr = (elf64_phdr_t*)((u64)ehdr + ehdr->e_phoff + ehdr->e_phentsize * i);
        if(phdr->p_type == PT_LOAD)
        {
            // 获取映射内存段开始位置
            u64 start_va = phdr->p_vaddr;
            // 获取映射内存段结束位置
            proc->ustack = start_va + phdr->p_memsz;
            //  转换elf的可读，可写，可执行的 flags
            u8 map_perm = PTE_U | flags_to_mmap_prot(phdr->p_flags);
            // 获取映射内存大小,需要向上对齐
            u64 map_size = PGROUNDUP(phdr->p_memsz);
            for (size_t j = 0; j < map_size; j += PAGE_SIZE)
            {
                // 分配物理内存，加载程序段，然后映射
                PhysPageNum ppn = kalloc();
                //获取到分配的物理内存的地址
                u64 paddr = phys_addr_from_phys_page_num(ppn).value;
                memcpy((void*)paddr, (void*)((u64)ehdr + phdr->p_offset + j), PAGE_SIZE);
                //内存逻辑段内存映射
                PageTable_map(&proc->pagetable, virt_addr_from_size_t(start_va + j), \
                              phys_addr_from_size_t(paddr), PAGE_SIZE, map_perm);
            }
        }
    }

    // 计算用户栈位置与应用数据总大小(base_size 供 fork/exit 释放页表使用)
    proc->ustack = 2 * PAGE_SIZE + PGROUNDUP(proc->ustack);
    proc->base_size = proc->ustack;
}

void load_app(size_t app_id)
{
    //加载ELF文件
    AppMetadata metadata = get_app_data(app_id + 1);

    //ELF 文件头
    elf64_ehdr_t *ehdr = (elf64_ehdr_t*)metadata.start;

    //校验 ELF 魔数与架构
    elf_check(ehdr);

    //创建任务
    TaskControlBlock* proc = task_create_pt(app_id);

    //加载程序段
    load_segment(ehdr, proc);

    //记录APP程序的入口地址，为 main 函数地址
    proc->entry = (u64)ehdr->e_entry;

    // 映射应用程序用户栈开始地址
    proc_ustack(proc);
}