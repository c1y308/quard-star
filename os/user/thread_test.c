#include <timeros/types.h>
#include <timeros/syscall.h>
#include <timeros/string.h>

static volatile int shared_count;
static volatile int local_ok[2];
static volatile int timer_seen;

static int timer_worker(u64 arg)
{
    timer_seen = 1;
    return (int)arg;
}

static int worker(u64 arg)
{
    int index = arg == 11 ? 0 : 1;
    int local = (int)arg;
    for (int i = 0; i < 80; i++) {
        local += 3;
        __atomic_fetch_add(&shared_count, 1, __ATOMIC_SEQ_CST);
        if ((i & 3) == 0) sys_yield();
        for (volatile int spin = 0; spin < 20000; spin++) { }
    }
    local_ok[index] = (local == (int)arg + 240);
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
    if (sys_gettid() != 0 || sys_waittid(0) != -1 ||
        sys_waittid(999) != -1 || sys_thread_create((void *)1, 0) != -1)
        goto fail;
    int first = sys_thread_create(worker, 11);
    int second = sys_thread_create(worker, 22);
    if (first <= 0 || second <= 0 || first == second) goto fail;
    if (join(first) != 11 || join(second) != 22) goto fail;
    if (shared_count != 160 || !local_ok[0] || !local_ok[1]) goto fail;
    int reused = sys_thread_create(worker, 11);
    if (reused != first || join(reused) != 11 || shared_count != 240) goto fail;
    for (int cycle = 0; cycle < 8; cycle++) {
        int tid = sys_thread_create(worker, 11);
        if (tid != first || join(tid) != 11) goto fail;
    }
    if (shared_count != 880) goto fail;
    int timer_tid = sys_thread_create(timer_worker, 7);
    if (timer_tid != first) goto fail;
    for (volatile int spin = 0; spin < 5000000 && !timer_seen; spin++) { }
    if (!timer_seen || join(timer_tid) != 7) goto fail;
    printf("THREAD_TEST_PASS pid=%d tid=%d reused=%d\n", sys_getpid(), sys_gettid(), reused);
    sys_exit(0);
fail:
    printf("THREAD_TEST_FAIL pid=%d count=%d\n", sys_getpid(), shared_count);
    sys_exit(1);
    return 1;
}
