#ifndef FAT32_H
#define FAT32_H

#include "block.h"
#include "vfs.h"

typedef struct fat32_fs fat32_fs_t;

vfs_node_t* fat32_mount_device(block_dev_t* dev);

#endif /* FAT32_H */
