#include <timeros/syscall.h>
#include <timeros/fs.h>
#include <timeros/stdio.h>

static char source[8192];
static char target[8192];

int main(void)
{
    if (sys_read(31, target, 1) != -1 || sys_write(31, source, 1) != (uint64_t)-1)
        sys_exit(1);
    for (int i = 0; i < sizeof(source); ++i) source[i] = (char)(i * 13);
    int fd = sys_open("fdshare", FS_CREATE | FS_RDWR);
    if (sys_open((const char *)0x40000000, FS_RDONLY) != -1 ||
        sys_write(fd, (const char *)0x40000000, 1) != (uint64_t)-1 ||
        sys_read(fd, (char *)0x40000000, 1) != -1) sys_exit(10);
    if (fd < 3 || sys_write(fd, source + 67, 6000) != 6000 || sys_close(fd) < 0)
        sys_exit(2);
    fd = sys_open("fdshare", FS_RDONLY);
    if (fd < 3 || sys_read(fd, target + 91, 6000) != 6000) sys_exit(3);
    for (int i = 0; i < 6000; ++i)
        if (target[i + 91] != source[i + 67]) sys_exit(4);
    if (sys_read(fd, target, 1) != 0 || sys_close(fd) < 0) sys_exit(5);

    fd = sys_open("fdshare", FS_RDONLY);
    if (fd < 3) sys_exit(6);
    int pid = sys_fork();
    if (pid < 0) sys_exit(7);
    if (pid == 0) {
        if (sys_read(fd, target, 1) != 1 || target[0] != source[67]) sys_exit(8);
        sys_exit(0);
    }
    if (sys_waitpid() != pid || sys_read(fd, target, 1) != 1 ||
        target[0] != source[68] || sys_close(fd) < 0) sys_exit(9);
    printf("fd test passed!\n");
    sys_exit(0);
    return 0;
}
