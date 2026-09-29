#ifndef TOS_LOADER_H__
#define TOS_LOADER_H__

#include <timeros/types.h>
#include <timeros/assert.h>
#include <timeros/stdio.h>


// 假设这个结构用于存储应用程序元数据
typedef struct {
    uint64_t start;
    uint64_t size;
    int id;      // app id; get_app_data_by_name 未找到时置 -1, 供 exec 判定
} AppMetadata;

#define EI_NIDENT 16
//所有的ASCII码都可以用“\”加数字（一般是8进制数字）来表示。
#define ELFMAG 0x464C457FU   // 0x464C457FU  "\177ELF"

#define EM_RISCV 0xF3  //risc-v 对应的 e_machine 的value值

#define EI_CLASS     4 //EI_NIDENT 的 第四位
#define ELFCLASSNONE 0
#define ELFCLASS32   1
#define ELFCLASS64   2
#define ELFCLASSNUM  3

#define PT_LOAD 1

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4


/**
 * @brief  elf header 结构体定义
 */
typedef struct {
    u8 e_ident[EI_NIDENT];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u64 e_entry;
    u64 e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} elf64_ehdr_t;

/**
 * @brief  program header 结构体定义
 */
typedef struct {
    u32 p_type;
    u32 p_flags;
    u64 p_offset;
    u64 p_vaddr;
    u64 p_paddr;
    u64 p_filesz;
    u64 p_memsz;
    u64 p_align;
} elf64_phdr_t;
// 获取加载的app数量
size_t get_num_app();
AppMetadata  get_app_data(size_t app_id);
/* 按名字查找 app 元数据(exec 使用), 未找到时 id==-1 */
AppMetadata  get_app_data_by_name(char* path);
void load_app(size_t app_id);
void get_app_names();
/* 校验 ELF 魔数与架构 */
void elf_check(elf64_ehdr_t *ehdr);
/* 解析 program header 并装载各 PT_LOAD 段到 proc 页表, 计算 ustack/base_size */
struct TaskControlBlock;
void load_segment(elf64_ehdr_t *ehdr, struct TaskControlBlock* proc);

#endif