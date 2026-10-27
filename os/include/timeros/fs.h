#ifndef TIMEROS_FS_H
#define TIMEROS_FS_H

#include <timeros/easyfs.h>

typedef struct ProcessControlBlock ProcessControlBlock;

#define FS_RDONLY 0U
#define FS_WRONLY 1U
#define FS_RDWR 2U
#define FS_CREATE (1U << 9)
#define FS_TRUNC (1U << 10)

void fs_init(void);
EasyFs *fs_instance(void);
void fs_fd_init(ProcessControlBlock *p);
void fs_fd_clone(ProcessControlBlock *dst, ProcessControlBlock *src);
void fs_fd_close_all(ProcessControlBlock *p);
int fs_open(ProcessControlBlock *p, const char *name, unsigned flags);
int fs_close(ProcessControlBlock *p, int fd);
efs_ssize fs_read(ProcessControlBlock *p, int fd, void *dst, efs_size len);
efs_ssize fs_write(ProcessControlBlock *p, int fd, const void *src, efs_size len);

#endif
