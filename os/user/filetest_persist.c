#include <timeros/syscall.h>
#include <timeros/fs.h>
#include <timeros/string.h>

int main(void)
{
    char buffer[14] = {0};
    int fd = sys_open("filea", FS_RDONLY);
    if (fd < 3 || sys_read(fd, buffer, sizeof(buffer)) != sizeof(buffer) ||
        sys_close(fd) < 0 || strcmp(buffer, "Hello, world!") != 0) sys_exit(1);
    printf("persistent file passed!\n");
    sys_exit(0);
    return 0;
}
