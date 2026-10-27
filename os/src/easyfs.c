#include <timeros/easyfs.h>

typedef char efs_inode_size_check[sizeof(EfsDiskInode) == 128 ? 1 : -1];
typedef char efs_dirent_size_check[sizeof(EfsDirEntry) == 32 ? 1 : -1];
typedef char efs_super_size_check[sizeof(EfsSuper) == 24 ? 1 : -1];

static void bytes_copy(void *d, const void *s, efs_size n)
{
    unsigned char *dst = d;
    const unsigned char *src = s;
    for (efs_size i = 0; i < n; ++i) dst[i] = src[i];
}

static void bytes_zero(void *d, efs_size n)
{
    unsigned char *dst = d;
    for (efs_size i = 0; i < n; ++i) dst[i] = 0;
}

static int valid_name(const char *name)
{
    if (!name) return 0;
    int n = 0;
    while (name[n]) {
        if (++n > EFS_NAME_MAX || name[n - 1] == '/') return 0;
    }
    return n > 0;
}

static int names_equal(const char *a, const char *b)
{
    for (int i = 0; i <= EFS_NAME_MAX; ++i) {
        if (a[i] != b[i]) return 0;
        if (!a[i]) return 1;
    }
    return 0;
}

static void layout(EasyFs *fs)
{
    fs->inode_start = 1 + fs->super.inode_bitmap_blocks;
    fs->data_bitmap_start = fs->inode_start + fs->super.inode_area_blocks;
    fs->data_start = fs->data_bitmap_start + fs->super.data_bitmap_blocks;
}

static int read_inode(EasyFs *fs, efs_u32 id, EfsDiskInode *out)
{
    if (id >= fs->super.inode_bitmap_blocks * 4096U) return -1;
    efs_u32 block = fs->inode_start + id / 4;
    if (block >= fs->data_bitmap_start) return -1;
    unsigned char buf[EFS_BLOCK_SIZE];
    if (fs->read(fs->ctx, block, buf) < 0) return -1;
    bytes_copy(out, buf + (id % 4) * sizeof(*out), sizeof(*out));
    return 0;
}

static int write_inode(EasyFs *fs, efs_u32 id, const EfsDiskInode *inode)
{
    efs_u32 block = fs->inode_start + id / 4;
    if (block >= fs->data_bitmap_start) return -1;
    unsigned char buf[EFS_BLOCK_SIZE];
    if (fs->read(fs->ctx, block, buf) < 0) return -1;
    bytes_copy(buf + (id % 4) * sizeof(*inode), inode, sizeof(*inode));
    return fs->write(fs->ctx, block, buf);
}

int efs_inode(EasyFs *fs, efs_u32 id, EfsDiskInode *inode)
{
    return read_inode(fs, id, inode);
}

static int bitmap_set(EasyFs *fs, efs_u32 start, efs_u32 bit, int value)
{
    unsigned char buf[EFS_BLOCK_SIZE];
    efs_u32 block = start + bit / 4096;
    if (fs->read(fs->ctx, block, buf) < 0) return -1;
    unsigned char mask = (unsigned char)(1U << (bit % 8));
    if (value) buf[(bit % 4096) / 8] |= mask;
    else buf[(bit % 4096) / 8] &= (unsigned char)~mask;
    return fs->write(fs->ctx, block, buf);
}

static int bitmap_alloc(EasyFs *fs, efs_u32 start, efs_u32 count,
                        efs_u32 limit, efs_u32 *bit)
{
    unsigned char buf[EFS_BLOCK_SIZE];
    for (efs_u32 block = 0; block < count; ++block) {
        if (fs->read(fs->ctx, start + block, buf) < 0) return -1;
        for (efs_u32 i = 0; i < EFS_BLOCK_SIZE && block * 4096 + i * 8 < limit; ++i) {
            if (buf[i] == 0xff) continue;
            for (int j = 0; j < 8; ++j) {
                efs_u32 pos = block * 4096 + i * 8 + j;
                if (pos >= limit) return -1;
                if (!(buf[i] & (1U << j))) {
                    buf[i] |= (unsigned char)(1U << j);
                    if (fs->write(fs->ctx, start + block, buf) < 0) return -1;
                    *bit = pos;
                    return 0;
                }
            }
        }
    }
    return -1;
}

static int alloc_data(EasyFs *fs, efs_u32 *block)
{
    efs_u32 bit;
    if (bitmap_alloc(fs, fs->data_bitmap_start, fs->super.data_bitmap_blocks,
                     fs->super.data_area_blocks, &bit) < 0) return -1;
    *block = fs->data_start + bit;
    unsigned char zero[EFS_BLOCK_SIZE];
    bytes_zero(zero, sizeof(zero));
    if (fs->write(fs->ctx, *block, zero) < 0) {
        bitmap_set(fs, fs->data_bitmap_start, bit, 0);
        return -1;
    }
    return 0;
}

static int free_data(EasyFs *fs, efs_u32 block)
{
    if (block < fs->data_start || block >= fs->super.total_blocks) return -1;
    return bitmap_set(fs, fs->data_bitmap_start, block - fs->data_start, 0);
}

static efs_u32 data_count(efs_u32 size)
{
    return (size + EFS_BLOCK_SIZE - 1) / EFS_BLOCK_SIZE;
}

static efs_u32 total_count(efs_u32 size)
{
    efs_u32 n = data_count(size), total = n;
    if (n > EFS_DIRECT) ++total;
    if (n > EFS_DIRECT + EFS_INDIRECT) {
        efs_u32 double_count = n - EFS_DIRECT - EFS_INDIRECT;
        total += 1 + (double_count + EFS_INDIRECT - 1) / EFS_INDIRECT;
    }
    return total;
}

static int enough_data(EasyFs *fs, efs_u32 needed)
{
    if (!needed) return 1;
    unsigned char buf[EFS_BLOCK_SIZE];
    efs_u32 free = 0, limit = fs->super.data_area_blocks;
    for (efs_u32 b = 0; b < fs->super.data_bitmap_blocks; ++b) {
        if (fs->read(fs->ctx, fs->data_bitmap_start + b, buf) < 0) return 0;
        for (efs_u32 i = 0; i < 4096 && b * 4096 + i < limit; ++i)
            if (!(buf[i / 8] & (1U << (i % 8))) && ++free >= needed) return 1;
    }
    return needed == 0;
}

static int indirect_get(EasyFs *fs, efs_u32 block, efs_u32 index, efs_u32 *value)
{
    if (block < fs->data_start || block >= fs->super.total_blocks || index >= EFS_INDIRECT) return -1;
    unsigned char buf[EFS_BLOCK_SIZE];
    if (fs->read(fs->ctx, block, buf) < 0) return -1;
    bytes_copy(value, buf + index * 4, 4);
    return 0;
}

static int indirect_set(EasyFs *fs, efs_u32 block, efs_u32 index, efs_u32 value)
{
    if (block < fs->data_start || block >= fs->super.total_blocks || index >= EFS_INDIRECT) return -1;
    unsigned char buf[EFS_BLOCK_SIZE];
    if (fs->read(fs->ctx, block, buf) < 0) return -1;
    bytes_copy(buf + index * 4, &value, 4);
    return fs->write(fs->ctx, block, buf);
}

static int block_for(EasyFs *fs, EfsDiskInode *inode, efs_u32 index,
                     int create, efs_u32 *block)
{
    if (index >= EFS_MAX_DATA_BLOCKS) return -1;
    if (index < EFS_DIRECT) {
        efs_u32 *slot = &inode->direct[index];
        if (create && !*slot && alloc_data(fs, slot) < 0) return -1;
        *block = *slot;
        return *block ? 0 : -1;
    }
    index -= EFS_DIRECT;
    efs_u32 *top = index < EFS_INDIRECT ? &inode->indirect1 : &inode->indirect2;
    if (index >= EFS_INDIRECT) index -= EFS_INDIRECT;
    if (create && !*top && alloc_data(fs, top) < 0) return -1;
    if (!*top) return -1;
    efs_u32 leaf = *top;
    if (top == &inode->indirect2) {
        efs_u32 pos = index / EFS_INDIRECT, child;
        if (indirect_get(fs, leaf, pos, &child) < 0) return -1;
        if (create && !child) {
            if (alloc_data(fs, &child) < 0 || indirect_set(fs, leaf, pos, child) < 0) return -1;
        }
        leaf = child;
        index %= EFS_INDIRECT;
    }
    if (!leaf || indirect_get(fs, leaf, index, block) < 0) return -1;
    if (create && !*block) {
        if (alloc_data(fs, block) < 0 || indirect_set(fs, leaf, index, *block) < 0) return -1;
    }
    return *block ? 0 : -1;
}

int efs_mount(EasyFs *fs)
{
    unsigned char buf[EFS_BLOCK_SIZE];
    if (!fs || !fs->read || !fs->write || fs->read(fs->ctx, 0, buf) < 0) return -1;
    bytes_copy(&fs->super, buf, sizeof(fs->super));
    EfsSuper *s = &fs->super;
    unsigned long long data_start = 1ULL + s->inode_bitmap_blocks +
        s->inode_area_blocks + s->data_bitmap_blocks;
    if (data_start > 0xffffffffU) return -1;
    layout(fs);
    if (s->magic != EFS_MAGIC || s->total_blocks < 8 || !s->inode_bitmap_blocks ||
        !s->inode_area_blocks || !s->data_bitmap_blocks || !s->data_area_blocks ||
        fs->data_start >= s->total_blocks ||
        s->data_area_blocks != s->total_blocks - fs->data_start ||
        s->inode_area_blocks < (unsigned long long)s->inode_bitmap_blocks * 1024U ||
        s->data_area_blocks > (unsigned long long)s->data_bitmap_blocks * 4096U) return -1;
    EfsDiskInode root;
    if (read_inode(fs, 0, &root) < 0 || root.type != EFS_DIRECTORY) return -1;
    return 0;
}

int efs_format(EasyFs *fs, efs_u32 blocks)
{
    if (!fs || !fs->read || !fs->write || blocks < 2048) return -1;
    efs_u32 inode_bitmap = 1, inode_area = 1024;
    efs_u32 data_total = blocks - 1 - inode_bitmap - inode_area;
    efs_u32 data_bitmap = (data_total + 4096) / 4097;
    fs->super = (EfsSuper){EFS_MAGIC, blocks, inode_bitmap, inode_area,
                           data_bitmap, data_total - data_bitmap};
    layout(fs);
    unsigned char zero[EFS_BLOCK_SIZE];
    bytes_zero(zero, sizeof(zero));
    for (efs_u32 b = 0; b < fs->data_start; ++b)
        if (fs->write(fs->ctx, b, zero) < 0) return -1;
    bytes_copy(zero, &fs->super, sizeof(fs->super));
    if (fs->write(fs->ctx, 0, zero) < 0 ||
        bitmap_set(fs, 1, 0, 1) < 0) return -1;
    EfsDiskInode root;
    bytes_zero(&root, sizeof(root));
    root.type = EFS_DIRECTORY;
    return write_inode(fs, 0, &root);
}

efs_ssize efs_read_at(EasyFs *fs, efs_u32 id, efs_u32 offset, void *dst, efs_size len)
{
    EfsDiskInode inode;
    if (read_inode(fs, id, &inode) < 0) return -1;
    if (offset >= inode.size) return 0;
    if (len > inode.size - offset) len = inode.size - offset;
    unsigned char buf[EFS_BLOCK_SIZE];
    efs_size done = 0;
    while (done < len) {
        efs_u32 pos = offset + done, block;
        efs_size n = EFS_BLOCK_SIZE - pos % EFS_BLOCK_SIZE;
        if (n > len - done) n = len - done;
        if (block_for(fs, &inode, pos / EFS_BLOCK_SIZE, 0, &block) < 0 ||
            fs->read(fs->ctx, block, buf) < 0) return -1;
        bytes_copy((unsigned char *)dst + done, buf + pos % EFS_BLOCK_SIZE, n);
        done += n;
    }
    return (efs_ssize)done;
}

efs_ssize efs_write_at(EasyFs *fs, efs_u32 id, efs_u32 offset, const void *src, efs_size len)
{
    EfsDiskInode inode;
    if (read_inode(fs, id, &inode) < 0 || offset > inode.size ||
        len > 0xffffffffU - offset || offset + len > EFS_MAX_DATA_BLOCKS * EFS_BLOCK_SIZE) return -1;
    efs_u32 end = offset + len;
    if (end > inode.size && !enough_data(fs, total_count(end) - total_count(inode.size))) return -1;
    unsigned char buf[EFS_BLOCK_SIZE];
    efs_size done = 0;
    while (done < len) {
        efs_u32 pos = offset + done, block;
        efs_size n = EFS_BLOCK_SIZE - pos % EFS_BLOCK_SIZE;
        if (n > len - done) n = len - done;
        if (block_for(fs, &inode, pos / EFS_BLOCK_SIZE, 1, &block) < 0 ||
            fs->read(fs->ctx, block, buf) < 0) return -1;
        bytes_copy(buf + pos % EFS_BLOCK_SIZE, (const unsigned char *)src + done, n);
        if (fs->write(fs->ctx, block, buf) < 0) return -1;
        done += n;
        if (pos + n > inode.size) inode.size = pos + n;
        if (write_inode(fs, id, &inode) < 0) return -1;
    }
    return (efs_ssize)done;
}

int efs_lookup(EasyFs *fs, const char *name, efs_u32 *id)
{
    if (!valid_name(name)) return -1;
    EfsDiskInode root;
    if (read_inode(fs, 0, &root) < 0 || root.type != EFS_DIRECTORY ||
        root.size % sizeof(EfsDirEntry)) return -1;
    for (efs_u32 off = 0; off < root.size; off += sizeof(EfsDirEntry)) {
        EfsDirEntry entry;
        if (efs_read_at(fs, 0, off, &entry, sizeof(entry)) != sizeof(entry)) return -1;
        if (names_equal(entry.name, name)) {
            *id = entry.inode_number;
            return 0;
        }
    }
    return -1;
}

int efs_create(EasyFs *fs, const char *name, efs_u32 *id)
{
    if (!valid_name(name) || efs_lookup(fs, name, id) == 0) return -1;
    EfsDiskInode root;
    if (read_inode(fs, 0, &root) < 0 || root.type != EFS_DIRECTORY) return -1;
    efs_u32 new_id;
    if (bitmap_alloc(fs, 1, fs->super.inode_bitmap_blocks,
                     fs->super.inode_bitmap_blocks * 4096U, &new_id) < 0) return -1;
    EfsDiskInode inode;
    bytes_zero(&inode, sizeof(inode));
    inode.type = EFS_FILE;
    if (write_inode(fs, new_id, &inode) < 0) goto fail;
    EfsDirEntry entry;
    bytes_zero(&entry, sizeof(entry));
    for (int i = 0; name[i]; ++i) entry.name[i] = name[i];
    entry.inode_number = new_id;
    if (efs_write_at(fs, 0, root.size, &entry, sizeof(entry)) != sizeof(entry)) goto fail;
    *id = new_id;
    return 0;
fail:
    bitmap_set(fs, 1, new_id, 0);
    return -1;
}

int efs_truncate(EasyFs *fs, efs_u32 id)
{
    EfsDiskInode inode;
    if (read_inode(fs, id, &inode) < 0 || inode.type != EFS_FILE) return -1;
    efs_u32 count = data_count(inode.size);
    for (efs_u32 i = 0; i < count; ++i) {
        efs_u32 block;
        if (block_for(fs, &inode, i, 0, &block) < 0 || free_data(fs, block) < 0) return -1;
    }
    if (inode.indirect1 && free_data(fs, inode.indirect1) < 0) return -1;
    if (inode.indirect2) {
        efs_u32 children = count > EFS_DIRECT + EFS_INDIRECT ?
            (count - EFS_DIRECT - EFS_INDIRECT + EFS_INDIRECT - 1) / EFS_INDIRECT : 0;
        for (efs_u32 i = 0; i < children; ++i) {
            efs_u32 block;
            if (indirect_get(fs, inode.indirect2, i, &block) < 0 || free_data(fs, block) < 0) return -1;
        }
        if (free_data(fs, inode.indirect2) < 0) return -1;
    }
    bytes_zero(&inode, sizeof(inode));
    inode.type = EFS_FILE;
    return write_inode(fs, id, &inode);
}
