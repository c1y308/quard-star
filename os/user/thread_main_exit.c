#include <timeros/types.h>
#include <timeros/syscall.h>
#include <timeros/string.h>

static int worker(u64 arg)
{
    if (arg == 77 && sys_gettid() != 0) {
        printf("MAIN_EXIT_PASS pid=%d tid=%d\n", sys_getpid(), sys_gettid());
        return 0;
    }
    printf("MAIN_EXIT_FAIL\n");
    return 1;
}

int main(void)
{
    if (sys_thread_create(worker, 77) < 0) {
        printf("MAIN_EXIT_FAIL create\n");
        sys_exit(1);
    }
    sys_thread_exit(0);
    return 1;
}
