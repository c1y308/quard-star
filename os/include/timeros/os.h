#ifndef TOS_OS_H__
#define TOS_OS_H__


#include <stdarg.h>
#include <timeros/types.h>
#include <timeros/address.h>  // 将 address.h 移到 task.h 之前
#include <timeros/assert.h>
#include <timeros/context.h>
#include <timeros/riscv.h>
#include <timeros/sbi.h>
#include <timeros/stack.h>
#include <timeros/stdio.h>
#include <timeros/string.h>
#include <timeros/syscall.h>
#include <timeros/task.h>     // 现在 task.h 可以访问 PageTable 了


/* trap.c */
extern void trap_init();


/* kerneltrap.S*/
extern void __alltraps(void);
extern void __restore(TrapContext *next);


/* switch.S */
extern void __switch(TaskContext *current_task_cx_ptr,TaskContext* next_task_cx_ptr );

/* task.c */
extern void schedule();
extern void task_create(void (*task_entry)(void));
extern void run_first_task();

/* app.c */
extern void task_init();

/* timer.c */
void sbi_set_timer(uint64_t stime_value);
void timer_init();
void set_next_trigger();
extern uint64_t get_time_us();
#endif /* __OS_H__ */