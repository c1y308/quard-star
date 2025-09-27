#ifndef TOS_TASK_H__
#define TOS_TASK_H__
#include "timeros/os.h"  // 直接包含定义 PageTable 的头文件
typedef enum TaskState
{
	UnInit, // 未初始化
    Ready, // 准备运行
    Running, // 正在运行
    Exited, // 已退出
}TaskState;

typedef struct TaskControlBlock
{
    TaskState task_state; 
    TaskContext task_context; 
    u64  kstack;                //应用内核栈的虚拟地址
}TaskControlBlock;

void proc_mapstacks(PageTable *kpgtbl);

#endif



