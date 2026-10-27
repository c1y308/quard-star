#ifndef TOS_TASK_H__
#define TOS_TASK_H__

#include <timeros/types.h>
#include <timeros/address.h>
#include <timeros/context.h>

/* 最大任务数, 由 task.c 移至此处, 便于 loader.c 等模块共享 */
#define MAX_TASKS 10
#define MAX_THREADS 32
#define MAX_FD 32
typedef struct OpenFile OpenFile;

typedef enum TaskState
{
	UnInit, // 未初始化
    Ready, // 准备运行
    Running, // 正在运行
    Zombie, // 已退出，等待 waittid 或父进程 wait 回收
}TaskState;

typedef struct ProcessControlBlock
{
    int pid;
    struct ProcessControlBlock* parent;
    PageTable pagetable;
    u64 base_size;              // ELF 映射区结束地址（页对齐）
    u64 ustack_base;            // 各线程用户栈槽位的基址
    u64 entry;
    int exit_code;
    int live_threads;
    bool used;
    bool zombie;
    bool tid_used[MAX_THREADS];
    OpenFile *fds[MAX_FD];
} ProcessControlBlock;

typedef struct TaskControlBlock
{
    TaskState task_state;
    int tid;
    ProcessControlBlock* process;
    TaskContext task_context;
    u64 trap_cx_paddr;
    u64 trap_cx_va;
    u64 kstack;                 // 内核栈顶
    u64 kstack_paddr;
    u64 ustack;                 // 用户栈顶
    int exit_code;
} TaskControlBlock;

/* 初始化应用程序 */
void app_init(void);
/* 获取当前执行应用程序trap上下文地址 */
u64 get_current_trap_cx();
/*返回当前执行的应用程序的satp token*/
u64 current_thread_user_token();
/* 任务调度*/
void schedule();
/* 启动第一个任务*/
void run_first_task();
/* 初始化所有进程控制块为 UnInit */
void procinit();
TaskControlBlock* current_thread(void);
/* 返回当前进程控制块 */
ProcessControlBlock* current_proc(void);
/* 分配新的 pid */
int allocpid();
/* 分配一个新进程，建立用户页表并映射跳板页 */
ProcessControlBlock* allocproc(void);
/* fork: 复制当前进程 */
int __sys_fork();
/* exec: 在当前进程中装载并运行名为 name 的应用 */
int exec(const char* name);
/* 释放进程资源并重置控制块 */
void freeproc(ProcessControlBlock* p);
/* 把 p 的子进程改挂到 initproc(processes[0]) */
void children_proc_clear(ProcessControlBlock* p);
/* 退出当前进程并调度下一个 */
void exit_current_and_run_next(u64 exit_code);
/* 等待任一子进程退出, 返回其 pid; 无子进程返回 -1 */
int wait();
int thread_create(u64 entry, u64 arg, u64 return_stub);
void thread_exit(int code);
int waittid(u64 tid);
#endif
