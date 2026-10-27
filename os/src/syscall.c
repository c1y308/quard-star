#include <timeros/os.h>
#include <timeros/fs.h>

static int user_page(u64 va, int write, unsigned char **physical)
{
    if (va >= TRAMPOLINE)
        return -1;
    PageTableEntry *pte = find_pte(&current_proc()->pagetable,
                                   floor_virts(virt_addr_from_size_t(va)));
    u64 required = PTE_V | PTE_U | (write ? PTE_W : PTE_R);
    if (!pte || (pte->bits & required) != required) return -1;
    *physical = (unsigned char *)(PTE2PA(pte->bits) + (va & (PAGE_SIZE - 1)));
    return 0;
}

static int user_range(u64 va, size_t len, int write)
{
    if (va >= TRAMPOLINE || len > TRAMPOLINE - va)
        return -1;
    for (u64 page = va & ~(PAGE_SIZE - 1); page < va + len; page += PAGE_SIZE) {
        unsigned char *physical;
        if (user_page(page, write, &physical) < 0) return -1;
    }
    return 0;
}

static int copy_to_user(u64 dst, const void *src, size_t len)
{
    size_t done = 0;
    while (done < len) {
        unsigned char *physical;
        if (user_page(dst + done, 1, &physical) < 0) return -1;
        size_t n = PAGE_SIZE - ((dst + done) & (PAGE_SIZE - 1));
        if (n > len - done) n = len - done;
        memcpy(physical, (const unsigned char *)src + done, n);
        done += n;
    }
    return 0;
}

static int copy_from_user(void *dst, u64 src, size_t len)
{
    size_t done = 0;
    while (done < len) {
        unsigned char *physical;
        if (user_page(src + done, 0, &physical) < 0) return -1;
        size_t n = PAGE_SIZE - ((src + done) & (PAGE_SIZE - 1));
        if (n > len - done) n = len - done;
        memcpy((unsigned char *)dst + done, physical, n);
        done += n;
    }
    return 0;
}

static int user_name(u64 va, char name[EFS_NAME_MAX + 1])
{
    for (int i = 0; i <= EFS_NAME_MAX; ++i) {
        if (copy_from_user(&name[i], va + i, 1) < 0) return -1;
        if (!name[i]) return i ? 0 : -1;
    }
    return -1;
}

static efs_ssize syscall_read(size_t fd, u64 user, size_t len)
{
    // Reading a file writes into the user buffer.
    if (fd >= MAX_FD || !current_proc()->fds[fd] || user_range(user, len, 1) < 0)
        return -1;

    unsigned char buf[EFS_BLOCK_SIZE];
    size_t done = 0;
    while (done < len) {
        size_t n = len - done;
        if (n > sizeof(buf)) n = sizeof(buf);
        efs_ssize result = fs_read(current_proc(), fd, buf, n);
        if (result < 0) return done ? (efs_ssize)done : -1;
        if (result && copy_to_user(user + done, buf, result) < 0) return -1;
        done += result;
        if ((efs_size)result < n) break;
    }
    return done;
}

static efs_ssize syscall_write(size_t fd, u64 user, size_t len)
{
    // Writing a file reads from the user buffer.
    if (fd >= MAX_FD || !current_proc()->fds[fd] || user_range(user, len, 0) < 0)
        return -1;

    unsigned char buf[EFS_BLOCK_SIZE];
    size_t done = 0;
    while (done < len) {
        size_t n = len - done;
        if (n > sizeof(buf)) n = sizeof(buf);
        if (copy_from_user(buf, user + done, n) < 0) return -1;
        efs_ssize result = fs_write(current_proc(), fd, buf, n);
        if (result < 0) return done ? (efs_ssize)done : -1;
        done += result;
        if ((efs_size)result < n) break;
    }
    return done;
}

uint64_t __SYSCALL(size_t id, reg_t arg1, reg_t arg2, reg_t arg3)
{
    char name[EFS_NAME_MAX + 1];
    switch (id) {
    case __NR_write:
        return syscall_write(arg1, arg2, arg3);
    case __NR_read:
        return syscall_read(arg1, arg2, arg3);
    case __NR_open:
        return user_name(arg1, name) < 0 ? (u64)-1 :
               (u64)(long)fs_open(current_proc(), name, arg2);
    case __NR_close:
        return fs_close(current_proc(), arg1);
    case __NR_sched_yield:
        schedule();
        return 0;
    case __NR_gettimeofday:
        return get_time_us();
    case __NR_exit:
        exit_current_and_run_next(arg1);
        return 0;
    case __NR_clone:
        return __sys_fork();
    case __NR_execve:
        return user_name(arg2, name) < 0 ? (u64)-1 : (u64)(long)exec(name);
    case __NR_waitid:
        return wait();
    case __NR_getpid:
        return current_proc()->pid;
    case __NR_thread_create:
        return thread_create(arg1, arg2, arg3);
    case __NR_gettid:
        return current_thread()->tid;
    case __NR_waittid:
        return waittid(arg1);
    case __NR_thread_exit:
        thread_exit((int)arg1);
        return 0;
    default:
        return (u64)-1;
    }
}
