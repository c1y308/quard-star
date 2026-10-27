#include <timeros/os.h>
#include <timeros/fs.h>

#define OPEN_FILES 64
#define FILE_STDIN 1
#define FILE_STDOUT 2
#define FILE_DISK 3

struct OpenFile {
    int refs;
    int kind;
    efs_u32 inode;
    efs_u32 offset;
    unsigned flags;
};

static EasyFs root_fs;
static OpenFile files[OPEN_FILES];
static OpenFile standard_in = {.kind = FILE_STDIN};
static OpenFile standard_out = {.kind = FILE_STDOUT};

static int disk_read(void *ctx, efs_u32 block, unsigned char *dst)
{
    (void)ctx;
    struct buf *b = bread(1, (int)block);
    memcpy(dst, b->data, EFS_BLOCK_SIZE);
    brelse(b);
    return 0;
}

static int disk_write(void *ctx, efs_u32 block, const unsigned char *src)
{
    (void)ctx;
    struct buf *b = bread(1, (int)block);
    memcpy(b->data, src, EFS_BLOCK_SIZE);
    bwrite(b);
    brelse(b);
    return 0;
}

void fs_init(void)
{
    root_fs.ctx = 0;
    root_fs.read = disk_read;
    root_fs.write = disk_write;
    if (efs_mount(&root_fs) < 0) panic("invalid easy-fs image");
    printk("easy-fs mounted: %d blocks\n", root_fs.super.total_blocks);
}

EasyFs *fs_instance(void) { return &root_fs; }

void fs_fd_init(ProcessControlBlock *p)
{
    for (int i = 0; i < MAX_FD; ++i) p->fds[i] = 0;
    p->fds[0] = &standard_in;
    p->fds[1] = &standard_out;
    p->fds[2] = &standard_out;
}

void fs_fd_clone(ProcessControlBlock *dst, ProcessControlBlock *src)
{
    for (int i = 0; i < MAX_FD; ++i) {
        dst->fds[i] = src->fds[i];
        if (dst->fds[i] && dst->fds[i]->kind == FILE_DISK) ++dst->fds[i]->refs;
    }
}

int fs_close(ProcessControlBlock *p, int fd)
{
    if (!p || fd < 0 || fd >= MAX_FD || !p->fds[fd]) return -1;
    OpenFile *file = p->fds[fd];
    p->fds[fd] = 0;
    if (file->kind == FILE_DISK && --file->refs == 0) memset(file, 0, sizeof(*file));
    return 0;
}

void fs_fd_close_all(ProcessControlBlock *p)
{
    for (int i = 0; i < MAX_FD; ++i) if (p->fds[i]) fs_close(p, i);
}

int fs_open(ProcessControlBlock *p, const char *name, unsigned flags)
{
    unsigned mode = flags & 3U;
    if (!p || mode > FS_RDWR || (flags & ~(3U | FS_CREATE | FS_TRUNC))) return -1;
    int fd = -1, slot = -1;
    for (int i = 3; i < MAX_FD; ++i) if (!p->fds[i]) { fd = i; break; }
    for (int i = 0; i < OPEN_FILES; ++i) if (!files[i].refs) { slot = i; break; }
    if (fd < 0 || slot < 0) return -1;
    efs_u32 id;
    if (efs_lookup(&root_fs, name, &id) < 0) {
        if (!(flags & FS_CREATE) || efs_create(&root_fs, name, &id) < 0) return -1;
    } else if (flags & (FS_CREATE | FS_TRUNC)) {
        if (efs_truncate(&root_fs, id) < 0) return -1;
    }
    EfsDiskInode inode;
    if (efs_inode(&root_fs, id, &inode) < 0 || inode.type != EFS_FILE) return -1;
    OpenFile *file = &files[slot];
    file->refs = 1;
    file->kind = FILE_DISK;
    file->inode = id;
    file->offset = 0;
    file->flags = flags;
    p->fds[fd] = file;
    return fd;
}

efs_ssize fs_read(ProcessControlBlock *p, int fd, void *dst, efs_size len)
{
    if (!p || fd < 0 || fd >= MAX_FD || !p->fds[fd])
        return -1;
    OpenFile *file = p->fds[fd];
    if (file->kind == FILE_STDIN) {
        if (!len)
            return 0;
        ((unsigned char *)dst)[0] = uart0_getchar();
        return 1;
    }
    if (file->kind != FILE_DISK || (file->flags & 3U) == FS_WRONLY) return -1;
    efs_ssize n = efs_read_at(&root_fs, file->inode, file->offset, dst, len);
    if (n > 0) file->offset += n;
    return n;
}

efs_ssize fs_write(ProcessControlBlock *p, int fd, const void *src, efs_size len)
{
    if (!p || fd < 0 || fd >= MAX_FD || !p->fds[fd]) return -1;
    OpenFile *file = p->fds[fd];
    if (file->kind == FILE_STDOUT) {
        const unsigned char *buf = src;
        for (efs_size i = 0; i < len; ++i) sbi_console_putchar(buf[i]);
        return len;
    }
    if (file->kind != FILE_DISK || (file->flags & 3U) == FS_RDONLY) return -1;
    efs_ssize n = efs_write_at(&root_fs, file->inode, file->offset, src, len);
    if (n > 0) file->offset += n;
    return n;
}
