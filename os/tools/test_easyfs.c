#include <timeros/easyfs.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCKS 2048U

static int memory_read(void *ctx, efs_u32 block, unsigned char *dst)
{
    if (block >= BLOCKS) return -1;
    memcpy(dst, (unsigned char *)ctx + block * EFS_BLOCK_SIZE, EFS_BLOCK_SIZE);
    return 0;
}

static int memory_write(void *ctx, efs_u32 block, const unsigned char *src)
{
    if (block >= BLOCKS) return -1;
    memcpy((unsigned char *)ctx + block * EFS_BLOCK_SIZE, src, EFS_BLOCK_SIZE);
    return 0;
}

int main(void)
{
    unsigned char *image = calloc(BLOCKS, EFS_BLOCK_SIZE);
    assert(image);
    EasyFs fs = {.ctx = image, .read = memory_read, .write = memory_write};
    assert(efs_mount(&fs) < 0);
    assert(efs_format(&fs, BLOCKS) == 0);
    assert(efs_mount(&fs) == 0);
    efs_u32 file;
    assert(efs_create(&fs, "large", &file) == 0);
    assert(efs_create(&fs, "large", &file) < 0);
    assert(efs_create(&fs, "name-is-more-than-twenty-seven-chars", &file) < 0);
    assert(efs_lookup(&fs, "large", &file) == 0);

    /* Cross both indirect boundaries and write across partial blocks. */
    enum { SIZE = (EFS_DIRECT + EFS_INDIRECT + 4) * EFS_BLOCK_SIZE + 17 };
    unsigned char *input = malloc(SIZE), *output = malloc(SIZE);
    assert(input && output);
    for (int i = 0; i < SIZE; ++i) input[i] = (unsigned char)(i * 37);
    assert(efs_write_at(&fs, file, 0, input, SIZE) == SIZE);
    EasyFs mounted = {.ctx = image, .read = memory_read, .write = memory_write};
    assert(efs_mount(&mounted) == 0);
    efs_u32 found;
    assert(efs_lookup(&mounted, "large", &found) == 0 && found == file);
    assert(efs_read_at(&mounted, found, 0, output, SIZE) == SIZE);
    assert(memcmp(input, output, SIZE) == 0);
    assert(efs_read_at(&mounted, found, SIZE, output, 1) == 0);
    assert(efs_truncate(&mounted, found) == 0);
    assert(efs_read_at(&mounted, found, 0, output, SIZE) == 0);
    assert(efs_write_at(&mounted, found, 0, input, SIZE) == SIZE);
    assert(efs_read_at(&mounted, found, 0, output, SIZE) == SIZE);
    assert(memcmp(input, output, SIZE) == 0);

    /* The requested extension must fail without changing the old size. */
    efs_u32 other;
    assert(efs_create(&mounted, "other", &other) == 0);
    unsigned char *too_large = calloc(1100, EFS_BLOCK_SIZE);
    assert(too_large);
    assert(efs_write_at(&mounted, other, 0, too_large, 1100UL * EFS_BLOCK_SIZE) < 0);
    EfsDiskInode inode;
    assert(efs_inode(&mounted, other, &inode) == 0 && inode.size == 0);
    free(input);
    free(output);
    free(too_large);
    free(image);
    puts("easy-fs tests passed");
    return 0;
}
