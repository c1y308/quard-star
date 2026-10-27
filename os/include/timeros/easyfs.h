#ifndef TIMEROS_EASYFS_H
#define TIMEROS_EASYFS_H

/* The layout is the on-disk layout of rCore Tutorial v3 easy-fs. */
#define EFS_BLOCK_SIZE 512
#define EFS_MAGIC 0x3b800001U
#define EFS_DIRECT 28
#define EFS_INDIRECT (EFS_BLOCK_SIZE / 4)
#define EFS_MAX_DATA_BLOCKS (EFS_DIRECT + EFS_INDIRECT + EFS_INDIRECT * EFS_INDIRECT)
#define EFS_NAME_MAX 27
#define EFS_FILE 0
#define EFS_DIRECTORY 1

typedef unsigned int efs_u32;
typedef unsigned long efs_size;
typedef long efs_ssize;

typedef struct {
    efs_u32 magic, total_blocks, inode_bitmap_blocks, inode_area_blocks;
    efs_u32 data_bitmap_blocks, data_area_blocks;
} EfsSuper;

typedef struct {
    efs_u32 size;
    efs_u32 direct[EFS_DIRECT];
    efs_u32 indirect1, indirect2, type;
} EfsDiskInode;

typedef struct {
    char name[28];
    efs_u32 inode_number;
} EfsDirEntry;

typedef struct {
    void *ctx;
    int (*read)(void *ctx, efs_u32 block, unsigned char *dst);
    int (*write)(void *ctx, efs_u32 block, const unsigned char *src);
    EfsSuper super;
    efs_u32 inode_start, data_bitmap_start, data_start;
} EasyFs;

int efs_format(EasyFs *fs, efs_u32 total_blocks);
int efs_mount(EasyFs *fs);
int efs_lookup(EasyFs *fs, const char *name, efs_u32 *inode_id);
int efs_create(EasyFs *fs, const char *name, efs_u32 *inode_id);
int efs_truncate(EasyFs *fs, efs_u32 inode_id);
efs_ssize efs_read_at(EasyFs *fs, efs_u32 inode_id, efs_u32 offset,
                      void *dst, efs_size len);
efs_ssize efs_write_at(EasyFs *fs, efs_u32 inode_id, efs_u32 offset,
                       const void *src, efs_size len);
int efs_inode(EasyFs *fs, efs_u32 inode_id, EfsDiskInode *inode);

#endif
