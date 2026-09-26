#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_MOUNTPOINT  0x04

#define VFS_O_RDONLY    0x01
#define VFS_O_WRONLY    0x02
#define VFS_O_RDWR      0x03
#define VFS_O_CREAT     0x04
#define VFS_O_TRUNC     0x08
#define VFS_O_APPEND    0x10

typedef struct vfs_dirent {
    char name[64];
    uint32_t size;
    uint8_t is_dir;
    uint32_t inode;
} vfs_dirent_t;

typedef struct vfs_stat {
    uint32_t size;
    uint8_t is_dir;
    uint32_t inode;
} vfs_stat_t;

struct vfs_node;

typedef int (*vfs_read_fn)(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
typedef int (*vfs_write_fn)(struct vfs_node* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
typedef int (*vfs_open_fn)(struct vfs_node* node, uint32_t flags);
typedef int (*vfs_close_fn)(struct vfs_node* node);
typedef int (*vfs_readdir_fn)(struct vfs_node* node, uint32_t index, vfs_dirent_t* dirent);
typedef struct vfs_node* (*vfs_finddir_fn)(struct vfs_node* node, const char* name);
typedef int (*vfs_mkdir_fn)(struct vfs_node* parent, const char* name);
typedef int (*vfs_create_fn)(struct vfs_node* parent, const char* name, uint32_t flags);
typedef int (*vfs_delete_fn)(struct vfs_node* parent, const char* name);
typedef int (*vfs_rename_fn)(struct vfs_node* parent, const char* old_name, const char* new_name);
typedef int (*vfs_stat_fn)(struct vfs_node* node, vfs_stat_t* st);

typedef struct vfs_node {
    char name[64];
    uint32_t flags;
    uint32_t size;
    uint32_t inode;
    uint32_t ref_count;

    vfs_read_fn read;
    vfs_write_fn write;
    vfs_open_fn open;
    vfs_close_fn close;
    vfs_readdir_fn readdir;
    vfs_finddir_fn finddir;
    vfs_mkdir_fn mkdir;
    vfs_create_fn create;
    vfs_delete_fn delete;
    vfs_rename_fn rename;
    vfs_stat_fn stat;

    void* priv_data;
    struct vfs_node* mount_target;
} vfs_node_t;

typedef struct {
    char drive_letter;
    char label[32];
    char fs_type[16];
    vfs_node_t* root;
    uint8_t is_mounted;
} vfs_drive_t;

void vfs_init(void);
int vfs_mount(char drive, vfs_node_t* root_node, const char* label, const char* fs_type);
int vfs_unmount(char drive);
vfs_drive_t* vfs_get_drive(char drive);
vfs_node_t* vfs_get_drive_root(char drive);

vfs_node_t* vfs_resolve_path(const char* current_path, const char* path);
int vfs_resolve_parent_and_leaf(const char* current_path, const char* path, vfs_node_t** out_parent, char* out_leaf);

int vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
int vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
int vfs_close(vfs_node_t* node);
int vfs_readdir(vfs_node_t* node, uint32_t index, vfs_dirent_t* dirent);
vfs_node_t* vfs_finddir(vfs_node_t* node, const char* name);
int vfs_create(vfs_node_t* parent, const char* name, uint32_t flags);
int vfs_mkdir(vfs_node_t* parent, const char* name);
int vfs_delete(vfs_node_t* parent, const char* name);
int vfs_mkdir_path(const char* current_path, const char* path);
int vfs_create_file(const char* current_path, const char* path);
int vfs_delete_path(const char* current_path, const char* path);
int vfs_rename_path(const char* current_path, const char* old_path, const char* new_path);

#endif /* VFS_H */
