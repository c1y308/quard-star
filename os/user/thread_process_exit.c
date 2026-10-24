#include <timeros/types.h>
#include <timeros/syscall.h>
#include <timeros/string.h>

static int worker(u64 arg)
{
    (void)arg;
    for (;;) sys_yield();
}

int main(void)
{
    if (sys_thread_create(worker, 0) < 0) {
        printf("PROCESS_EXIT_FAIL create\n");
        sys_exit(1);
    }
    printf("PROCESS_EXIT_START pid=%d\n", sys_getpid());
    sys_exit(23);
    return 1;
}
