#include "ata.h"

#define ATA_PRIMARY_IO   0x1F0
#define ATA_PRIMARY_CTRL 0x3F6

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT   0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF   0x20
#define ATA_SR_DSC  0x10
#define ATA_SR_DRQ  0x08
#define ATA_SR_CORR 0x04
#define ATA_SR_IDX  0x02
#define ATA_SR_ERR  0x01

#define ATA_CMD_READ_PIO   0x20
#define ATA_CMD_WRITE_PIO  0x30
#define ATA_CMD_IDENTIFY   0xEC
#define ATA_CMD_CACHE_FLUSH 0xE7

static ata_drive_t ata_drives[2];

static inline uint8_t ata_inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void ata_outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t ata_inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile ("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void ata_outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static void ata_io_wait(uint16_t ctrl_base) {
    /* 400ns delay via 4 reads of alternate status */
    ata_inb(ctrl_base);
    ata_inb(ctrl_base);
    ata_inb(ctrl_base);
    ata_inb(ctrl_base);
}

static int ata_wait_ready(uint16_t io_base, uint16_t ctrl_base) {
    ata_io_wait(ctrl_base);
    for (int i = 0; i < 100000; i++) {
        uint8_t status = ata_inb(io_base + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) {
            return 0;
        }
    }
    return -1;
}

static int ata_wait_drq(uint16_t io_base, uint16_t ctrl_base) {
    ata_io_wait(ctrl_base);
    for (int i = 0; i < 100000; i++) {
        uint8_t status = ata_inb(io_base + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DF) return -1;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
            return 0;
        }
    }
    return -1;
}

static void ata_identify_drive(int drive_index, uint16_t io_base, uint16_t ctrl_base, uint8_t drive_sel) {
    ata_drive_t* drive = &ata_drives[drive_index];
    drive->io_base = io_base;
    drive->ctrl_base = ctrl_base;
    drive->drive_num = drive_index;
    drive->is_present = 0;

    /* Select drive */
    ata_outb(io_base + ATA_REG_HDDEVSEL, drive_sel);
    ata_io_wait(ctrl_base);

    /* Reset sector count and LBA registers */
    ata_outb(io_base + ATA_REG_SECCOUNT, 0);
    ata_outb(io_base + ATA_REG_LBA0, 0);
    ata_outb(io_base + ATA_REG_LBA1, 0);
    ata_outb(io_base + ATA_REG_LBA2, 0);

    /* Send IDENTIFY command */
    ata_outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_io_wait(ctrl_base);

    uint8_t status = ata_inb(io_base + ATA_REG_STATUS);
    if (status == 0 || status == 0xFF) {
        /* No device */
        return;
    }

    if (ata_wait_ready(io_base, ctrl_base) != 0) return;

    /* Check if non-ATA device (ATAPI) */
    uint8_t lba1 = ata_inb(io_base + ATA_REG_LBA1);
    uint8_t lba2 = ata_inb(io_base + ATA_REG_LBA2);
    if (lba1 == 0x14 && lba2 == 0xEB) return; /* ATAPI */
    if (lba1 == 0x69 && lba2 == 0x96) return; /* ATAPI */

    if (ata_wait_drq(io_base, ctrl_base) != 0) return;

    /* Read 256 16-bit words */
    uint16_t identify_buf[256];
    for (int i = 0; i < 256; i++) {
        identify_buf[i] = ata_inw(io_base + ATA_REG_DATA);
    }

    drive->is_present = 1;

    /* Extract Model String (Words 27..46 = 40 bytes) with byte-swap */
    int idx = 0;
    for (int i = 27; i <= 46; i++) {
        drive->block_dev.model[idx++] = (char)(identify_buf[i] >> 8);
        drive->block_dev.model[idx++] = (char)(identify_buf[i] & 0xFF);
    }
    drive->block_dev.model[39] = '\0';

    /* Trim trailing spaces from model */
    int len = 38;
    while (len >= 0 && drive->block_dev.model[len] == ' ') {
        drive->block_dev.model[len--] = '\0';
    }

    /* Extract Total 28-bit LBA Sectors (Words 60..61) */
    uint32_t sectors_28 = ((uint32_t)identify_buf[61] << 16) | identify_buf[60];
    drive->block_dev.total_sectors = sectors_28;
    drive->block_dev.sector_size = 512;

    if (drive_index == 0) {
        drive->block_dev.name[0] = 'a';
        drive->block_dev.name[1] = 't';
        drive->block_dev.name[2] = 'a';
        drive->block_dev.name[3] = '0';
        drive->block_dev.name[4] = '\0';
    } else {
        drive->block_dev.name[0] = 'a';
        drive->block_dev.name[1] = 't';
        drive->block_dev.name[2] = 'a';
        drive->block_dev.name[3] = '1';
        drive->block_dev.name[4] = '\0';
    }

    drive->block_dev.read_sectors = ata_read_sectors;
    drive->block_dev.write_sectors = ata_write_sectors;
    drive->block_dev.priv_data = drive;

    block_dev_register(&drive->block_dev);
}

void ata_init(void) {
    /* Probe Primary Master (0xA0) */
    ata_identify_drive(0, ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, 0xA0);

    /* Probe Primary Slave (0xB0) */
    ata_identify_drive(1, ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, 0xB0);
}

int ata_read_sectors(block_dev_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !buffer || count == 0) return -1;
    ata_drive_t* drive = (ata_drive_t*)dev->priv_data;
    if (!drive || !drive->is_present) return -1;

    uint16_t io = drive->io_base;
    uint16_t ctrl = drive->ctrl_base;
    uint8_t drive_sel = (drive->drive_num == 0) ? 0xE0 : 0xF0;

    for (uint32_t s = 0; s < count; s++) {
        uint64_t cur_lba = lba + s;

        if (ata_wait_ready(io, ctrl) != 0) return -1;

        /* Send LBA and Sector Count */
        ata_outb(io + ATA_REG_HDDEVSEL, (uint8_t)(drive_sel | ((cur_lba >> 24) & 0x0F)));
        ata_outb(io + ATA_REG_SECCOUNT, 1);
        ata_outb(io + ATA_REG_LBA0, (uint8_t)(cur_lba & 0xFF));
        ata_outb(io + ATA_REG_LBA1, (uint8_t)((cur_lba >> 8) & 0xFF));
        ata_outb(io + ATA_REG_LBA2, (uint8_t)((cur_lba >> 16) & 0xFF));
        ata_outb(io + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

        if (ata_wait_drq(io, ctrl) != 0) return -1;

        /* Read 256 words (512 bytes) */
        uint16_t* ptr = (uint16_t*)(buffer + (s * 512));
        for (int i = 0; i < 256; i++) {
            ptr[i] = ata_inw(io + ATA_REG_DATA);
        }
    }

    return 0;
}

int ata_write_sectors(block_dev_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    if (!dev || !buffer || count == 0) return -1;
    ata_drive_t* drive = (ata_drive_t*)dev->priv_data;
    if (!drive || !drive->is_present) return -1;

    uint16_t io = drive->io_base;
    uint16_t ctrl = drive->ctrl_base;
    uint8_t drive_sel = (drive->drive_num == 0) ? 0xE0 : 0xF0;

    for (uint32_t s = 0; s < count; s++) {
        uint64_t cur_lba = lba + s;

        if (ata_wait_ready(io, ctrl) != 0) return -1;

        /* Send LBA and Sector Count */
        ata_outb(io + ATA_REG_HDDEVSEL, (uint8_t)(drive_sel | ((cur_lba >> 24) & 0x0F)));
        ata_outb(io + ATA_REG_SECCOUNT, 1);
        ata_outb(io + ATA_REG_LBA0, (uint8_t)(cur_lba & 0xFF));
        ata_outb(io + ATA_REG_LBA1, (uint8_t)((cur_lba >> 8) & 0xFF));
        ata_outb(io + ATA_REG_LBA2, (uint8_t)((cur_lba >> 16) & 0xFF));
        ata_outb(io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

        if (ata_wait_drq(io, ctrl) != 0) return -1;

        /* Write 256 words (512 bytes) */
        const uint16_t* ptr = (const uint16_t*)(buffer + (s * 512));
        for (int i = 0; i < 256; i++) {
            ata_outw(io + ATA_REG_DATA, ptr[i]);
        }

        /* Flush cache */
        ata_outb(io + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
        ata_wait_ready(io, ctrl);
    }

    return 0;
}

ata_drive_t* ata_get_drive(int index) {
    if (index >= 0 && index < 2 && ata_drives[index].is_present) {
        return &ata_drives[index];
    }
    return NULL;
}
