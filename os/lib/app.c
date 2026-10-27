#include <timeros/os.h>
uint64_t syscall(size_t id, reg_t arg1, reg_t arg2, reg_t arg3) {

    register uintptr_t a0 asm ("a0") = (uintptr_t)(arg1);
    register uintptr_t a1 asm ("a1") = (uintptr_t)(arg2);
    register uintptr_t a2 asm ("a2") = (uintptr_t)(arg3);
    register uintptr_t a7 asm ("a7") = (uintptr_t)(id);

    asm volatile ("ecall"
		      : "+r" (a0)
		      : "r" (a1), "r" (a2), "r" (a7)
		      : "memory");
    return a0;
}

uint64_t sys_write(size_t fd, const char* buf, size_t len)
{
    return syscall(__NR_write,fd,(reg_t)buf, len);
}

int sys_open(const char *name, unsigned flags)
{
    return (int)syscall(__NR_open, (reg_t)name, flags, 0);
}

int sys_close(int fd)
{
    return (int)syscall(__NR_close, fd, 0, 0);
}

uint64_t sys_yield()
{
    return syscall(__NR_sched_yield,0,0,0);
}

uint64_t sys_gettime()
{
    return syscall(__NR_gettimeofday,0,0,0);
}

int sys_read(size_t fd ,const char* buf , size_t len)
{
    return syscall(__NR_read,fd,(reg_t)buf, len);
}

/* 获取一个字符 */
char getchar()
{
    char data[1];
    sys_read(stdin,data,1);
    return data[0];
}

/* fork: 复制当前进程, 子进程返回 0, 父进程返回子进程 pid */
int sys_fork()
{
    return syscall(__NR_clone,0,0,0);
}

/* exec: 在当前进程中装载并运行名为 name 的应用 */
int sys_exec(char* name)
{
    return syscall(__NR_execve,0,(reg_t)name,0);
}

/* 等待任一子进程退出, 返回其 pid */
int sys_waitpid()
{
    return syscall(__NR_waitid,0,0,0);
}

/* 退出当前进程 */
int sys_exit(u64 exit_code)
{
    return syscall(__NR_exit,exit_code,0,0);
}

/* ra of a newly created user thread points here. The entry return value is a0. */
static void thread_return_stub(u64 code)
{
    sys_thread_exit((int)code);
    for (;;) sys_yield();
}

int sys_thread_create(int (*entry)(u64), u64 arg)
{
    return (int)syscall(__NR_thread_create, (reg_t)entry, arg,
                        (reg_t)thread_return_stub);
}

int sys_gettid(void) { return (int)syscall(__NR_gettid, 0, 0, 0); }
int sys_getpid(void) { return (int)syscall(__NR_getpid, 0, 0, 0); }
int sys_waittid(int tid) { return (int)syscall(__NR_waittid, tid, 0, 0); }
void sys_thread_exit(int code)
{
    syscall(__NR_thread_exit, code, 0, 0);
    for (;;) sys_yield();
}
