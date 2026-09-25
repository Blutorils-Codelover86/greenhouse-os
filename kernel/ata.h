#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <stddef.h>
#include "block.h"

typedef struct {
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t drive_num; /* 0 = Master (0xA0), 1 = Slave (0xB0) */
    uint8_t is_present;
    block_dev_t block_dev;
} ata_drive_t;

void ata_init(void);
int ata_read_sectors(block_dev_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer);
int ata_write_sectors(block_dev_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer);
ata_drive_t* ata_get_drive(int index);

#endif /* ATA_H */
