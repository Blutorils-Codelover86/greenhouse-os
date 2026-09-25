#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stddef.h>

#define MAX_BLOCK_DEVICES 8

typedef struct block_dev {
    char name[16];
    char model[40];
    uint64_t total_sectors;
    uint32_t sector_size;
    int (*read_sectors)(struct block_dev* dev, uint64_t lba, uint32_t count, uint8_t* buffer);
    int (*write_sectors)(struct block_dev* dev, uint64_t lba, uint32_t count, const uint8_t* buffer);
    void* priv_data;
} block_dev_t;

int block_dev_register(block_dev_t* dev);
block_dev_t* block_dev_get(int index);
block_dev_t* block_dev_find(const char* name);
int block_dev_count(void);

int block_read(block_dev_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer);
int block_write(block_dev_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer);

#endif /* BLOCK_H */
