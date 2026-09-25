#include "fat32.h"
#include "heap.h"

typedef struct {
    uint8_t  name[11];             /* 8.3 format */
    uint8_t  attr;                 /* 0x10 = DIR, 0x20 = ARCHIVE/FILE */
    uint8_t  nt_res;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_high;         /* High 16 bits of first cluster */
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t cluster_low;          /* Low 16 bits of first cluster */
    uint32_t file_size;
} __attribute__((packed)) fat32_dir_entry_t;

struct fat32_fs {
    block_dev_t* dev;
    uint64_t partition_lba;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t cluster_size_bytes;
    uint32_t reserved_sectors;
    uint32_t num_fats;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;
    uint64_t fat_lba;
    uint64_t data_lba;
};

typedef struct {
    fat32_fs_t* fs;
    uint32_t first_cluster;
    uint32_t current_cluster;
    uint32_t dir_cluster;         /* Parent directory cluster where this entry is located */
    uint32_t dir_entry_offset;    /* Byte offset of this entry in dir_cluster */
    uint8_t  is_dir;
    uint32_t file_size;
    vfs_node_t vfs_node;
} fat32_node_t;

static char f_toupper(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return c;
}

static size_t f_strlen(const char* s) {
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}

static int f_strcasecmp(const char* s1, const char* s2) {
    while (*s1 && (f_toupper(*s1) == f_toupper(*s2))) {
        s1++;
        s2++;
    }
    return (int)f_toupper(*(const unsigned char*)s1) - (int)f_toupper(*(const unsigned char*)s2);
}

static char* f_strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

static void* f_memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < count; i++) d[i] = s[i];
    return dest;
}

static void* f_memset(void* dest, int val, size_t count) {
    uint8_t* ptr = (uint8_t*)dest;
    for (size_t i = 0; i < count; i++) ptr[i] = (uint8_t)val;
    return dest;
}

static uint64_t fat32_cluster_to_lba(fat32_fs_t* fs, uint32_t cluster) {
    return fs->data_lba + (uint64_t)(cluster - 2) * fs->sectors_per_cluster;
}

static uint32_t fat32_get_next_cluster(fat32_fs_t* fs, uint32_t cluster) {
    uint64_t fat_sector = fs->fat_lba + ((uint64_t)cluster * 4) / 512;
    uint32_t fat_offset = (cluster * 4) % 512;

    uint8_t sector_buf[512];
    if (block_read(fs->dev, fat_sector, 1, sector_buf) != 0) {
        return 0x0FFFFFFF;
    }

    uint32_t next = *(uint32_t*)(sector_buf + fat_offset);
    return next & 0x0FFFFFFFU;
}

static int fat32_set_fat_entry(fat32_fs_t* fs, uint32_t cluster, uint32_t value) {
    uint64_t fat_offset_bytes = (uint64_t)cluster * 4;
    uint64_t sector_idx = fat_offset_bytes / 512;
    uint32_t offset_in_sec = (uint32_t)(fat_offset_bytes % 512);

    uint8_t sector_buf[512];

    /* Update each FAT copy */
    for (uint32_t i = 0; i < fs->num_fats; i++) {
        uint64_t sec_lba = fs->fat_lba + (i * fs->sectors_per_fat) + sector_idx;
        if (block_read(fs->dev, sec_lba, 1, sector_buf) != 0) return -1;

        uint32_t* entry = (uint32_t*)(sector_buf + offset_in_sec);
        *entry = (*entry & 0xF0000000) | (value & 0x0FFFFFFF);

        if (block_write(fs->dev, sec_lba, 1, sector_buf) != 0) return -1;
    }
    return 0;
}

static uint32_t fat32_allocate_cluster(fat32_fs_t* fs) {
    uint8_t sector_buf[512];

    for (uint32_t s = 0; s < fs->sectors_per_fat; s++) {
        if (block_read(fs->dev, fs->fat_lba + s, 1, sector_buf) != 0) return 0;

        uint32_t* entries = (uint32_t*)sector_buf;
        for (int i = 0; i < 128; i++) {
            uint32_t cluster_idx = (s * 128) + i;
            if (cluster_idx < 2) continue;

            if ((entries[i] & 0x0FFFFFFF) == 0) {
                fat32_set_fat_entry(fs, cluster_idx, 0x0FFFFFFFU);

                /* Zero the cluster sectors */
                uint8_t zero_buf[512];
                f_memset(zero_buf, 0, 512);
                uint64_t cl_lba = fat32_cluster_to_lba(fs, cluster_idx);
                for (uint32_t c = 0; c < fs->sectors_per_cluster; c++) {
                    block_write(fs->dev, cl_lba + c, 1, zero_buf);
                }

                return cluster_idx;
            }
        }
    }
    return 0; /* Out of disk space */
}

static void fat32_free_chain(fat32_fs_t* fs, uint32_t start_cluster) {
    uint32_t curr = start_cluster;
    while (curr >= 2 && curr < 0x0FFFFFF8U) {
        uint32_t next = fat32_get_next_cluster(fs, curr);
        fat32_set_fat_entry(fs, curr, 0);
        curr = next;
    }
}

/* Convert 8.3 raw FAT directory name to standard format "FILE.EXT" */
static void fat32_format_name(const uint8_t* raw, char* out) {
    int o = 0;
    int name_len = 8;
    while (name_len > 0 && raw[name_len - 1] == ' ') name_len--;

    for (int i = 0; i < name_len; i++) {
        out[o++] = (char)raw[i];
    }

    int ext_len = 3;
    while (ext_len > 0 && raw[8 + ext_len - 1] == ' ') ext_len--;

    if (ext_len > 0) {
        out[o++] = '.';
        for (int i = 0; i < ext_len; i++) {
            out[o++] = (char)raw[8 + i];
        }
    }
    out[o] = '\0';
}

/* Convert standard format "FILE.EXT" to 8.3 raw FAT directory name (11 bytes uppercase) */
static void fat32_to_83(const char* in, uint8_t* out) {
    f_memset(out, ' ', 11);
    const char* p = in;

    int i = 0;
    while (*p && *p != '.' && i < 8) {
        out[i++] = (uint8_t)f_toupper(*p++);
    }

    while (*p && *p != '.') p++;
    if (*p == '.') p++;

    i = 0;
    while (*p && i < 3) {
        out[8 + i++] = (uint8_t)f_toupper(*p++);
    }
}

static int fat32_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
static int fat32_write(vfs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
static int fat32_readdir(vfs_node_t* node, uint32_t index, vfs_dirent_t* dirent);
static vfs_node_t* fat32_finddir(vfs_node_t* node, const char* name);
static int fat32_mkdir(vfs_node_t* parent, const char* name);
static int fat32_create(vfs_node_t* parent, const char* name, uint32_t flags);
static int fat32_delete(vfs_node_t* parent, const char* name);
static int fat32_rename(vfs_node_t* parent, const char* old_name, const char* new_name);

static fat32_node_t* fat32_alloc_node(fat32_fs_t* fs, const char* name, uint8_t is_dir, uint32_t cluster, uint32_t size) {
    fat32_node_t* node = (fat32_node_t*)kmalloc(sizeof(fat32_node_t));
    if (!node) return NULL;
    f_memset(node, 0, sizeof(fat32_node_t));

    node->fs = fs;
    node->is_dir = is_dir;
    node->first_cluster = cluster;
    node->current_cluster = cluster;
    node->file_size = size;

    f_strncpy(node->vfs_node.name, name, 63);
    node->vfs_node.flags = is_dir ? VFS_DIRECTORY : VFS_FILE;
    node->vfs_node.size = size;
    node->vfs_node.inode = cluster;
    node->vfs_node.priv_data = node;

    node->vfs_node.read = fat32_read;
    node->vfs_node.write = fat32_write;
    node->vfs_node.readdir = fat32_readdir;
    node->vfs_node.finddir = fat32_finddir;
    node->vfs_node.mkdir = fat32_mkdir;
    node->vfs_node.create = fat32_create;
    node->vfs_node.delete = fat32_delete;
    node->vfs_node.rename = fat32_rename;

    return node;
}

static int fat32_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !buffer) return -1;
    fat32_node_t* fnode = (fat32_node_t*)node->priv_data;
    if (!fnode || fnode->is_dir) return -1;

    if (offset >= fnode->file_size) return 0;
    uint32_t bytes_to_read = size;
    if (offset + bytes_to_read > fnode->file_size) {
        bytes_to_read = fnode->file_size - offset;
    }

    fat32_fs_t* fs = fnode->fs;
    uint32_t cluster_size = fs->cluster_size_bytes;
    uint32_t cluster = fnode->first_cluster;

    /* Advance to start cluster */
    uint32_t cluster_offset = offset / cluster_size;
    for (uint32_t c = 0; c < cluster_offset; c++) {
        if (cluster < 2 || cluster >= 0x0FFFFFF8U) return 0;
        cluster = fat32_get_next_cluster(fs, cluster);
    }

    uint32_t in_cluster_offset = offset % cluster_size;
    uint32_t bytes_read = 0;
    uint8_t* cluster_buf = (uint8_t*)kmalloc(cluster_size);
    if (!cluster_buf) return -1;

    while (bytes_read < bytes_to_read && cluster >= 2 && cluster < 0x0FFFFFF8U) {
        uint64_t cl_lba = fat32_cluster_to_lba(fs, cluster);
        if (block_read(fs->dev, cl_lba, fs->sectors_per_cluster, cluster_buf) != 0) {
            kfree(cluster_buf);
            return (int)bytes_read;
        }

        uint32_t chunk = cluster_size - in_cluster_offset;
        if (chunk > (bytes_to_read - bytes_read)) {
            chunk = bytes_to_read - bytes_read;
        }

        f_memcpy(buffer + bytes_read, cluster_buf + in_cluster_offset, chunk);
        bytes_read += chunk;
        in_cluster_offset = 0;

        cluster = fat32_get_next_cluster(fs, cluster);
    }

    kfree(cluster_buf);
    return (int)bytes_read;
}

static int fat32_write(vfs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (!node || !buffer) return -1;
    fat32_node_t* fnode = (fat32_node_t*)node->priv_data;
    if (!fnode || fnode->is_dir) return -1;

    fat32_fs_t* fs = fnode->fs;
    uint32_t cluster_size = fs->cluster_size_bytes;

    /* If empty file, allocate first cluster */
    if (fnode->first_cluster < 2) {
        fnode->first_cluster = fat32_allocate_cluster(fs);
        if (fnode->first_cluster < 2) return -1;
    }

    uint32_t cluster = fnode->first_cluster;
    uint32_t cluster_idx = offset / cluster_size;

    /* Advance or extend cluster chain */
    for (uint32_t c = 0; c < cluster_idx; c++) {
        uint32_t next = fat32_get_next_cluster(fs, cluster);
        if (next < 2 || next >= 0x0FFFFFF8U) {
            next = fat32_allocate_cluster(fs);
            if (next < 2) return -1;
            fat32_set_fat_entry(fs, cluster, next);
        }
        cluster = next;
    }

    uint32_t in_cluster_offset = offset % cluster_size;
    uint32_t bytes_written = 0;
    uint8_t* cluster_buf = (uint8_t*)kmalloc(cluster_size);
    if (!cluster_buf) return -1;

    while (bytes_written < size) {
        uint64_t cl_lba = fat32_cluster_to_lba(fs, cluster);
        if (block_read(fs->dev, cl_lba, fs->sectors_per_cluster, cluster_buf) != 0) {
            kfree(cluster_buf);
            return (int)bytes_written;
        }

        uint32_t chunk = cluster_size - in_cluster_offset;
        if (chunk > (size - bytes_written)) {
            chunk = size - bytes_written;
        }

        f_memcpy(cluster_buf + in_cluster_offset, buffer + bytes_written, chunk);
        if (block_write(fs->dev, cl_lba, fs->sectors_per_cluster, cluster_buf) != 0) {
            kfree(cluster_buf);
            return (int)bytes_written;
        }

        bytes_written += chunk;
        in_cluster_offset = 0;

        if (bytes_written < size) {
            uint32_t next = fat32_get_next_cluster(fs, cluster);
            if (next < 2 || next >= 0x0FFFFFF8U) {
                next = fat32_allocate_cluster(fs);
                if (next < 2) break;
                fat32_set_fat_entry(fs, cluster, next);
            }
            cluster = next;
        }
    }

    kfree(cluster_buf);

    if (offset + bytes_written > fnode->file_size) {
        fnode->file_size = offset + bytes_written;
        node->size = fnode->file_size;

        /* Update directory entry on disk */
        if (fnode->dir_cluster >= 2) {
            uint64_t dir_lba = fat32_cluster_to_lba(fs, fnode->dir_cluster) + (fnode->dir_entry_offset / 512);
            uint32_t offset_in_sec = fnode->dir_entry_offset % 512;
            uint8_t sec_buf[512];
            if (block_read(fs->dev, dir_lba, 1, sec_buf) == 0) {
                fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sec_buf + offset_in_sec);
                entry->file_size = fnode->file_size;
                entry->cluster_low = (uint16_t)(fnode->first_cluster & 0xFFFF);
                entry->cluster_high = (uint16_t)((fnode->first_cluster >> 16) & 0xFFFF);
                block_write(fs->dev, dir_lba, 1, sec_buf);
            }
        }
    }

    return (int)bytes_written;
}

static int fat32_readdir(vfs_node_t* node, uint32_t index, vfs_dirent_t* dirent) {
    if (!node || !dirent) return -1;
    fat32_node_t* fnode = (fat32_node_t*)node->priv_data;
    if (!fnode || !fnode->is_dir) return -1;

    fat32_fs_t* fs = fnode->fs;
    uint32_t cluster_size = fs->cluster_size_bytes;
    uint32_t cluster = fnode->first_cluster;

    uint8_t* cluster_buf = (uint8_t*)kmalloc(cluster_size);
    if (!cluster_buf) return -1;

    uint32_t entry_count = 0;

    while (cluster >= 2 && cluster < 0x0FFFFFF8U) {
        uint64_t cl_lba = fat32_cluster_to_lba(fs, cluster);
        if (block_read(fs->dev, cl_lba, fs->sectors_per_cluster, cluster_buf) != 0) {
            kfree(cluster_buf);
            return -1;
        }

        uint32_t num_entries = cluster_size / sizeof(fat32_dir_entry_t);
        fat32_dir_entry_t* entries = (fat32_dir_entry_t*)cluster_buf;

        for (uint32_t i = 0; i < num_entries; i++) {
            if (entries[i].name[0] == 0x00) {
                kfree(cluster_buf);
                return -1; /* End of directory */
            }
            if (entries[i].name[0] == 0xE5) continue; /* Deleted */
            if (entries[i].attr == 0x0F) continue;   /* LFN entry */
            if (entries[i].attr & 0x08) continue;   /* Volume ID */

            if (entry_count == index) {
                fat32_format_name(entries[i].name, dirent->name);
                dirent->size = entries[i].file_size;
                dirent->is_dir = (entries[i].attr & 0x10) ? 1 : 0;
                dirent->inode = ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
                kfree(cluster_buf);
                return 0;
            }
            entry_count++;
        }

        cluster = fat32_get_next_cluster(fs, cluster);
    }

    kfree(cluster_buf);
    return -1;
}

static vfs_node_t* fat32_finddir(vfs_node_t* node, const char* name) {
    if (!node || !name) return NULL;
    fat32_node_t* fnode = (fat32_node_t*)node->priv_data;
    if (!fnode || !fnode->is_dir) return NULL;

    fat32_fs_t* fs = fnode->fs;
    uint32_t cluster_size = fs->cluster_size_bytes;
    uint32_t cluster = fnode->first_cluster;

    uint8_t* cluster_buf = (uint8_t*)kmalloc(cluster_size);
    if (!cluster_buf) return NULL;

    while (cluster >= 2 && cluster < 0x0FFFFFF8U) {
        uint64_t cl_lba = fat32_cluster_to_lba(fs, cluster);
        if (block_read(fs->dev, cl_lba, fs->sectors_per_cluster, cluster_buf) != 0) {
            kfree(cluster_buf);
            return NULL;
        }

        uint32_t num_entries = cluster_size / sizeof(fat32_dir_entry_t);
        fat32_dir_entry_t* entries = (fat32_dir_entry_t*)cluster_buf;

        for (uint32_t i = 0; i < num_entries; i++) {
            if (entries[i].name[0] == 0x00) {
                kfree(cluster_buf);
                return NULL;
            }
            if (entries[i].name[0] == 0xE5 || entries[i].attr == 0x0F) continue;

            char entry_name[64];
            fat32_format_name(entries[i].name, entry_name);

            if (f_strcasecmp(entry_name, name) == 0) {
                uint32_t file_cluster = ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
                uint8_t is_dir = (entries[i].attr & 0x10) ? 1 : 0;
                uint32_t fsize = entries[i].file_size;

                fat32_node_t* res_node = fat32_alloc_node(fs, entry_name, is_dir, file_cluster, fsize);
                if (res_node) {
                    res_node->dir_cluster = cluster;
                    res_node->dir_entry_offset = i * sizeof(fat32_dir_entry_t);
                }
                kfree(cluster_buf);
                return &res_node->vfs_node;
            }
        }

        cluster = fat32_get_next_cluster(fs, cluster);
    }

    kfree(cluster_buf);
    return NULL;
}

static int fat32_create_or_mkdir(vfs_node_t* parent, const char* name, uint8_t is_dir) {
    if (!parent || !name || *name == '\0') return -1;
    fat32_node_t* pnode = (fat32_node_t*)parent->priv_data;
    if (!pnode || !pnode->is_dir) return -1;

    fat32_fs_t* fs = pnode->fs;
    uint32_t cluster_size = fs->cluster_size_bytes;
    uint32_t cluster = pnode->first_cluster;

    uint8_t* cluster_buf = (uint8_t*)kmalloc(cluster_size);
    if (!cluster_buf) return -1;

    uint32_t target_cluster = 0;
    uint32_t target_entry_idx = 0;
    int found_slot = 0;

    while (cluster >= 2 && cluster < 0x0FFFFFF8U) {
        uint64_t cl_lba = fat32_cluster_to_lba(fs, cluster);
        if (block_read(fs->dev, cl_lba, fs->sectors_per_cluster, cluster_buf) != 0) {
            kfree(cluster_buf);
            return -1;
        }

        uint32_t num_entries = cluster_size / sizeof(fat32_dir_entry_t);
        fat32_dir_entry_t* entries = (fat32_dir_entry_t*)cluster_buf;

        for (uint32_t i = 0; i < num_entries; i++) {
            if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                target_cluster = cluster;
                target_entry_idx = i;
                found_slot = 1;
                break;
            }
        }
        if (found_slot) break;

        uint32_t next = fat32_get_next_cluster(fs, cluster);
        if (next < 2 || next >= 0x0FFFFFF8U) {
            /* Expand parent directory */
            next = fat32_allocate_cluster(fs);
            if (next < 2) {
                kfree(cluster_buf);
                return -1;
            }
            fat32_set_fat_entry(fs, cluster, next);
            target_cluster = next;
            target_entry_idx = 0;
            found_slot = 1;
            break;
        }
        cluster = next;
    }

    if (!found_slot) {
        kfree(cluster_buf);
        return -1;
    }

    /* Allocate cluster for new directory or file */
    uint32_t new_cluster = fat32_allocate_cluster(fs);
    if (new_cluster < 2) {
        kfree(cluster_buf);
        return -1;
    }

    /* If directory, initialize . and .. entries */
    if (is_dir) {
        uint8_t dir_init_buf[512];
        f_memset(dir_init_buf, 0, 512);
        fat32_dir_entry_t* d_entries = (fat32_dir_entry_t*)dir_init_buf;

        /* "." */
        f_memset(d_entries[0].name, ' ', 11);
        d_entries[0].name[0] = '.';
        d_entries[0].attr = 0x10;
        d_entries[0].cluster_low = (uint16_t)(new_cluster & 0xFFFF);
        d_entries[0].cluster_high = (uint16_t)((new_cluster >> 16) & 0xFFFF);

        /* ".." */
        f_memset(d_entries[1].name, ' ', 11);
        d_entries[1].name[0] = '.';
        d_entries[1].name[1] = '.';
        d_entries[1].attr = 0x10;
        d_entries[1].cluster_low = (uint16_t)(pnode->first_cluster & 0xFFFF);
        d_entries[1].cluster_high = (uint16_t)((pnode->first_cluster >> 16) & 0xFFFF);

        uint64_t nd_lba = fat32_cluster_to_lba(fs, new_cluster);
        block_write(fs->dev, nd_lba, 1, dir_init_buf);
    }

    /* Read the parent directory sector containing the slot */
    uint64_t slot_lba = fat32_cluster_to_lba(fs, target_cluster) + (target_entry_idx * sizeof(fat32_dir_entry_t)) / 512;
    uint32_t slot_sec_offset = (target_entry_idx * sizeof(fat32_dir_entry_t)) % 512;

    uint8_t sec_buf[512];
    if (block_read(fs->dev, slot_lba, 1, sec_buf) != 0) {
        kfree(cluster_buf);
        return -1;
    }

    fat32_dir_entry_t* new_entry = (fat32_dir_entry_t*)(sec_buf + slot_sec_offset);
    f_memset(new_entry, 0, sizeof(fat32_dir_entry_t));
    fat32_to_83(name, new_entry->name);
    new_entry->attr = is_dir ? 0x10 : 0x20;
    new_entry->cluster_low = (uint16_t)(new_cluster & 0xFFFF);
    new_entry->cluster_high = (uint16_t)((new_cluster >> 16) & 0xFFFF);
    new_entry->file_size = 0;

    block_write(fs->dev, slot_lba, 1, sec_buf);
    kfree(cluster_buf);
    return 0;
}

static int fat32_create(vfs_node_t* parent, const char* name, uint32_t flags) {
    (void)flags;
    return fat32_create_or_mkdir(parent, name, 0);
}

static int fat32_mkdir(vfs_node_t* parent, const char* name) {
    return fat32_create_or_mkdir(parent, name, 1);
}

static int fat32_delete(vfs_node_t* parent, const char* name) {
    if (!parent || !name) return -1;
    vfs_node_t* target = fat32_finddir(parent, name);
    if (!target) return -1;

    fat32_node_t* tnode = (fat32_node_t*)target->priv_data;
    fat32_fs_t* fs = tnode->fs;

    /* Free cluster chain */
    if (tnode->first_cluster >= 2) {
        fat32_free_chain(fs, tnode->first_cluster);
    }

    /* Mark directory entry as deleted (0xE5) */
    if (tnode->dir_cluster >= 2) {
        uint64_t dir_lba = fat32_cluster_to_lba(fs, tnode->dir_cluster) + (tnode->dir_entry_offset / 512);
        uint32_t offset_in_sec = tnode->dir_entry_offset % 512;
        uint8_t sec_buf[512];
        if (block_read(fs->dev, dir_lba, 1, sec_buf) == 0) {
            sec_buf[offset_in_sec] = 0xE5;
            block_write(fs->dev, dir_lba, 1, sec_buf);
        }
    }

    kfree(tnode);
    return 0;
}

static int fat32_rename(vfs_node_t* parent, const char* old_name, const char* new_name) {
    if (!parent || !old_name || !new_name) return -1;
    vfs_node_t* target = fat32_finddir(parent, old_name);
    if (!target) return -1;

    fat32_node_t* tnode = (fat32_node_t*)target->priv_data;
    fat32_fs_t* fs = tnode->fs;

    if (tnode->dir_cluster >= 2) {
        uint64_t dir_lba = fat32_cluster_to_lba(fs, tnode->dir_cluster) + (tnode->dir_entry_offset / 512);
        uint32_t offset_in_sec = tnode->dir_entry_offset % 512;
        uint8_t sec_buf[512];
        if (block_read(fs->dev, dir_lba, 1, sec_buf) == 0) {
            fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sec_buf + offset_in_sec);
            fat32_to_83(new_name, entry->name);
            block_write(fs->dev, dir_lba, 1, sec_buf);
            f_strncpy(target->name, new_name, 63);
            kfree(tnode);
            return 0;
        }
    }
    kfree(tnode);
    return -1;
}

vfs_node_t* fat32_mount_device(block_dev_t* dev) {
    if (!dev) return NULL;

    uint8_t sector0[512];
    if (block_read(dev, 0, 1, sector0) != 0) return NULL;

    uint64_t partition_lba = 0;
    uint8_t bpb_sector[512];
    int found_valid_bpb = 0;

    /* First check if Sector 0 itself is a FAT32 Volume Boot Record */
    uint16_t b0_bps = *(uint16_t*)&sector0[11];
    uint8_t  b0_spc = sector0[13];
    uint16_t b0_res = *(uint16_t*)&sector0[14];
    uint8_t  b0_num_fats = sector0[16];
    uint32_t b0_spf = *(uint32_t*)&sector0[36];

    if (b0_bps == 512 && b0_spc > 0 && b0_res > 0 && b0_num_fats > 0 && b0_spf > 0) {
        partition_lba = 0;
        f_memcpy(bpb_sector, sector0, 512);
        found_valid_bpb = 1;
    } else if (sector0[510] == 0x55 && sector0[511] == 0xAA) {
        /* Check 4 MBR partition table entries */
        for (int p = 0; p < 4; p++) {
            int entry_offset = 0x1BE + (p * 16);
            uint8_t part_type = sector0[entry_offset + 4];
            uint32_t start_lba = *(uint32_t*)&sector0[entry_offset + 8];

            if (start_lba != 0 && (part_type == 0x0B || part_type == 0x0C || part_type == 0x07 || part_type == 0x0E || part_type == 0x83)) {
                if (block_read(dev, start_lba, 1, bpb_sector) == 0) {
                    uint16_t bps = *(uint16_t*)&bpb_sector[11];
                    uint8_t  spc = bpb_sector[13];
                    uint16_t res = *(uint16_t*)&bpb_sector[14];
                    uint8_t  nf  = bpb_sector[16];
                    uint32_t spf = *(uint32_t*)&bpb_sector[36];
                    if (bps == 512 && spc > 0 && res > 0 && nf > 0 && spf > 0) {
                        partition_lba = start_lba;
                        found_valid_bpb = 1;
                        break;
                    }
                }
            }
        }
    }

    if (!found_valid_bpb) {
        return NULL;
    }

    /* Validate FAT32 BPB */
    uint16_t bytes_per_sector = *(uint16_t*)&bpb_sector[11];
    uint8_t  sectors_per_cluster = bpb_sector[13];
    uint16_t reserved_sectors = *(uint16_t*)&bpb_sector[14];
    uint8_t  num_fats = bpb_sector[16];
    uint32_t sectors_per_fat = *(uint32_t*)&bpb_sector[36];
    uint32_t root_cluster = *(uint32_t*)&bpb_sector[44];

    if (bytes_per_sector != 512 || sectors_per_cluster == 0 || reserved_sectors == 0 || num_fats == 0 || sectors_per_fat == 0) {
        return NULL;
    }

    fat32_fs_t* fs = (fat32_fs_t*)kmalloc(sizeof(fat32_fs_t));
    if (!fs) return NULL;

    fs->dev = dev;
    fs->partition_lba = partition_lba;
    fs->bytes_per_sector = bytes_per_sector;
    fs->sectors_per_cluster = sectors_per_cluster;
    fs->cluster_size_bytes = (uint32_t)bytes_per_sector * sectors_per_cluster;
    fs->reserved_sectors = reserved_sectors;
    fs->num_fats = num_fats;
    fs->sectors_per_fat = sectors_per_fat;
    fs->root_cluster = root_cluster;

    fs->fat_lba = partition_lba + reserved_sectors;
    fs->data_lba = fs->fat_lba + (uint64_t)num_fats * sectors_per_fat;

    fat32_node_t* root = fat32_alloc_node(fs, "", 1, root_cluster, 0);
    if (!root) {
        kfree(fs);
        return NULL;
    }

    return &root->vfs_node;
}
