#define _POSIX_C_SOURCE 200809L
#include <timeros/easyfs.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define IMAGE_BLOCKS 65536U

static int image_read(void *ctx, efs_u32 block, unsigned char *dst)
{
    FILE *file = ctx;
    return fseeko(file, (off_t)block * EFS_BLOCK_SIZE, SEEK_SET) == 0 &&
           fread(dst, 1, EFS_BLOCK_SIZE, file) == EFS_BLOCK_SIZE ? 0 : -1;
}

static int image_write(void *ctx, efs_u32 block, const unsigned char *src)
{
    FILE *file = ctx;
    return fseeko(file, (off_t)block * EFS_BLOCK_SIZE, SEEK_SET) == 0 &&
           fwrite(src, 1, EFS_BLOCK_SIZE, file) == EFS_BLOCK_SIZE ? 0 : -1;
}

static int copy_program(EasyFs *fs, const char *dir, const char *name)
{
    if (!*name || strlen(name) > EFS_NAME_MAX || strchr(name, '/')) return -1;
    char path[1024];
    if (snprintf(path, sizeof(path), "%s/%s", dir, name) >= (int)sizeof(path)) return -1;
    struct stat st;
    if (stat(path, &st) < 0 || !S_ISREG(st.st_mode)) return 0;
    FILE *input = fopen(path, "rb");
    if (!input) return -1;
    efs_u32 id;
    int result = 0;
    if (efs_lookup(fs, name, &id) < 0 && efs_create(fs, name, &id) < 0) result = -1;
    if (!result && efs_truncate(fs, id) < 0) result = -1;
    unsigned char buf[EFS_BLOCK_SIZE];
    efs_u32 offset = 0;
    while (!result) {
        size_t n = fread(buf, 1, sizeof(buf), input);
        if (!n) { if (ferror(input)) result = -1; break; }
        if (efs_write_at(fs, id, offset, buf, n) != (efs_ssize)n) { result = -1; break; }
        offset += n;
    }
    fclose(input);
    return result;
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "usage: %s IMAGE PROGRAM_DIR [--reset]\n", argv[0]);
        return 2;
    }
    int reset = argc == 4 && strcmp(argv[3], "--reset") == 0;
    if (argc == 4 && !reset) return 2;
    FILE *file = reset ? fopen(argv[1], "w+b") : fopen(argv[1], "r+b");
    int fresh = reset;
    if (!file && !reset && errno == ENOENT) {
        file = fopen(argv[1], "w+b");
        fresh = 1;
    }
    if (!file) { perror(argv[1]); return 1; }
    EasyFs fs = {.ctx = file, .read = image_read, .write = image_write};
    if (fresh) {
        if (ftruncate(fileno(file), (off_t)IMAGE_BLOCKS * EFS_BLOCK_SIZE) < 0 ||
            efs_format(&fs, IMAGE_BLOCKS) < 0) {
            fprintf(stderr, "failed to format %s\n", argv[1]);
            fclose(file);
            return 1;
        }
    } else if (efs_mount(&fs) < 0) {
        fprintf(stderr, "%s is not an easy-fs image; use --reset explicitly\n", argv[1]);
        fclose(file);
        return 1;
    }
    DIR *dir = opendir(argv[2]);
    if (!dir) { perror(argv[2]); fclose(file); return 1; }
    struct dirent *entry;
    int failed = 0;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;
        if (copy_program(&fs, argv[2], entry->d_name) < 0) {
            fprintf(stderr, "failed to import %s\n", entry->d_name);
            failed = 1;
            break;
        }
    }
    closedir(dir);
    if (fflush(file) < 0) failed = 1;
    fclose(file);
    return failed;
}
