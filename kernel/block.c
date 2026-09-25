#include "block.h"

static block_dev_t* block_devices[MAX_BLOCK_DEVICES] = {0};
static int num_block_devices = 0;

static int block_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int block_dev_register(block_dev_t* dev) {
    if (!dev || num_block_devices >= MAX_BLOCK_DEVICES) {
        return -1;
    }
    block_devices[num_block_devices++] = dev;
    return 0;
}

block_dev_t* block_dev_get(int index) {
    if (index >= 0 && index < num_block_devices) {
        return block_devices[index];
    }
    return NULL;
}

block_dev_t* block_dev_find(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < num_block_devices; i++) {
        if (block_devices[i] && block_strcmp(block_devices[i]->name, name) == 0) {
            return block_devices[i];
        }
    }
    return NULL;
}

int block_dev_count(void) {
    return num_block_devices;
}

int block_read(block_dev_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !dev->read_sectors || !buffer) return -1;
    return dev->read_sectors(dev, lba, count, buffer);
}

int block_write(block_dev_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    if (!dev || !dev->write_sectors || !buffer) return -1;
    return dev->write_sectors(dev, lba, count, buffer);
}
