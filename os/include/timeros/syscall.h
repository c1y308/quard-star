#include "types.h"
#include <stddef.h>

/* syscall */
uint64_t __SYSCALL(size_t syscall_id, reg_t arg1, reg_t arg2, reg_t arg3);

#define __NR_read 63
#define __NR_write 64
#define __NR_open 56
#define __NR_close 57
#define __NR_sched_yield 124
#define __NR_exit 93
#define __NR_gettimeofday 169
#define __NR_waitid 95
#define __NR_clone 220
#define __NR_execve 221
#define __NR_getpid 172
#define __NR_thread_create 1000
#define __NR_gettid 1001
#define __NR_waittid 1002
#define __NR_thread_exit 1003



uint64_t sys_write(size_t fd, const char* buf, size_t len);
int sys_open(const char *name, unsigned flags);
int sys_close(int fd);
uint64_t sys_yield();
uint64_t sys_gettime();
int sys_read(size_t fd ,const char* buf , size_t len);
char getchar();
int sys_fork();
int sys_exec(char* name);
int sys_waitpid();
int sys_exit(u64 exit_code);
int sys_thread_create(int (*entry)(u64), u64 arg);
int sys_gettid(void);
int sys_getpid(void);
int sys_waittid(int tid);
void sys_thread_exit(int code);
