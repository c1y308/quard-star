#include <timeros/types.h>
#include <timeros/syscall.h>
#include <timeros/fs.h>
#include <timeros/string.h>

int main(void)
{
    const char message[] = "Hello, world!";
    char buffer[sizeof(message)] = {0};
    int fd = sys_open("filea", FS_CREATE | FS_WRONLY);
    if (fd < 3 || sys_write(fd, message, sizeof(message)) != sizeof(message) ||
        sys_close(fd) < 0) sys_exit(1);
    fd = sys_open("filea", FS_RDONLY);
    if (fd < 3 || sys_read(fd, buffer, sizeof(buffer)) != sizeof(buffer) ||
        sys_read(fd, buffer, 1) != 0 || sys_close(fd) < 0) sys_exit(2);
    if (strcmp(buffer, message) != 0) sys_exit(3);
    printf("file_test passed!\n");
    sys_exit(0);
    return 0;
}
