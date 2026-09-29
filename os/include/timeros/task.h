#ifndef TOS_TASK_H__
#define TOS_TASK_H__

#include <timeros/os.h>

/* 最大任务数, 由 task.c 移至此处, 便于 loader.c 等模块共享 */
#define MAX_TASKS 10

typedef enum TaskState
{
	UnInit, // 未初始化
    Ready, // 准备运行
    Running, // 正在运行
    Zombie, // 已退出(僵尸态, 等待父进程 wait 回收)
}TaskState;

typedef struct TaskControlBlock
{
    TaskState task_state;       //任务状态
    int pid;                    //进程 ID
    struct TaskControlBlock* parent;  //父进程
    TaskContext task_context;   //任务上下文
    u64 trap_cx_ppn;            //Trap 上下文所在物理地址
    u64  base_size;             //应用数据大小
    u64  kstack;                //应用内核栈的虚拟地址
    u64  ustack;                //应用用户栈的虚拟地址
    u64  entry;                 //应用程序入口地址
    PageTable pagetable;        //应用页表所在物理页
    u64 exit_code;              //进程退出码
}TaskControlBlock;

/* 映射用户程序内核栈 */
void proc_mapstacks(PageTable* kpgtbl);
/* 创建应用页表 */
TaskControlBlock*  task_create_pt(size_t app_id);
/* 初始化应用程序 */
void app_init(size_t app_id);
/* 获取当前执行应用程序trap上下文地址 */
u64 get_current_trap_cx();
/*返回当前执行的应用程序的satp token*/
u64 current_user_token();
/* 任务调度*/
void schedule();
/* 启动第一个任务*/
void run_first_task();
/* 初始化所有进程控制块为 UnInit */
void procinit();
/* 映射应用程序用户栈 */
void proc_ustack(struct TaskControlBlock* p);
/* 返回当前进程控制块 */
struct TaskControlBlock* current_proc();
/* 分配新的 pid */
int allocpid();
/* 分配一个新进程(查找空闲槽, 建 trap 页与页表) */
struct TaskControlBlock* allocproc();
/* fork: 复制当前进程 */
int __sys_fork();
/* exec: 在当前进程中装载并运行名为 name 的应用 */
int exec(const char* name);
/* 释放进程资源并重置控制块 */
void freeproc(struct TaskControlBlock* p);
/* 把 p 的子进程改挂到 initproc(tasks[0]) */
void children_proc_clear(struct TaskControlBlock* p);
/* 退出当前进程并调度下一个 */
void exit_current_and_run_next(u64 exit_code);
/* 等待任一子进程退出, 返回其 pid; 无子进程返回 -1 */
int wait();
#endif



