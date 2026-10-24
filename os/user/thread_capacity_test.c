#include <timeros/types.h>
#include <timeros/syscall.h>
#include <timeros/string.h>

static volatile int release_workers;

static int waiting_worker(u64 arg)
{
    while (!release_workers) sys_yield();
    return (int)arg;
}

static int join(int tid)
{
    int code;
    while ((code = sys_waittid(tid)) == -2) sys_yield();
    return code;
}

int main(void)
{
    int tids[30];
    for (int i = 0; i < 30; i++) {
        tids[i] = sys_thread_create(waiting_worker, i);
        if (tids[i] != i + 1) goto fail;
    }
    if (sys_thread_create(waiting_worker, 99) != -1 ||
        sys_fork() != -1 || sys_exec("time") != -1) goto fail;
    release_workers = 1;
    for (int i = 0; i < 30; i++)
        if (join(tids[i]) != i) goto fail;
    int reused = sys_thread_create(waiting_worker, 42);
    if (reused != 1 || join(reused) != 42) goto fail;
    printf("CAPACITY_TEST_PASS pid=%d reused=%d\n", sys_getpid(), reused);
    sys_exit(0);
fail:
    printf("CAPACITY_TEST_FAIL\n");
    sys_exit(1);
    return 1;
}
