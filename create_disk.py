#!/usr/bin/env python3
import struct
import os

def make_fat32_disk(filename="disk.img", size_mb=64):
    total_sectors = (size_mb * 1024 * 1024) // 512
    bytes_per_sector = 512
    sectors_per_cluster = 1
    reserved_sectors = 32
    num_fats = 2
    
    # Calculate FAT size
    # Total data sectors approx = total_sectors - reserved - 2*fat_size
    # clusters approx = total_data_sectors / sectors_per_cluster
    # fat_size = (clusters * 4 + 511) / 512
    # For 64MB: 131072 sectors -> ~1009 sectors per FAT
    sectors_per_fat = 1024
    root_cluster = 2

    # Initialize disk buffer
    disk = bytearray(size_mb * 1024 * 1024)

    # 1. Volume Boot Record (VBR) at sector 0
    vbr = bytearray(512)
    vbr[0:3] = b'\xeb\x58\x90' # JMP short
    vbr[3:11] = b'MSWIN4.1'
    struct.pack_into('<H', vbr, 11, bytes_per_sector)
    vbr[13] = sectors_per_cluster
    struct.pack_into('<H', vbr, 14, reserved_sectors)
    vbr[16] = num_fats
    struct.pack_into('<H', vbr, 17, 0) # Root entries (0 for FAT32)
    struct.pack_into('<H', vbr, 19, 0) # Small sectors
    vbr[21] = 0xF8 # Media descriptor (Fixed disk)
    struct.pack_into('<H', vbr, 22, 0) # Sectors per FAT (FAT12/16)
    struct.pack_into('<H', vbr, 24, 63) # Sectors per track
    struct.pack_into('<H', vbr, 26, 255) # Heads
    struct.pack_into('<I', vbr, 28, 0) # Hidden sectors
    struct.pack_into('<I', vbr, 32, total_sectors) # Large total sectors
    struct.pack_into('<I', vbr, 36, sectors_per_fat) # Sectors per FAT
    struct.pack_into('<H', vbr, 40, 0) # Extended flags
    struct.pack_into('<H', vbr, 42, 0) # FS Version
    struct.pack_into('<I', vbr, 44, root_cluster) # Root dir cluster
    struct.pack_into('<H', vbr, 48, 1) # FSInfo sector
    struct.pack_into('<H', vbr, 50, 6) # Backup boot sector
    vbr[64] = 0x80 # Drive number
    vbr[66] = 0x29 # Boot signature
    struct.pack_into('<I', vbr, 67, 0x12345678) # Volume ID
    vbr[71:82] = b'GREENHOUSE ' # Volume label
    vbr[82:90] = b'FAT32   ' # Filesystem type
    vbr[510:512] = b'\x55\xaa'

    disk[0:512] = vbr
    # Backup boot sector at sector 6
    disk[6*512:7*512] = vbr

    # 2. FSInfo Sector at sector 1
    fsinfo = bytearray(512)
    fsinfo[0:4] = b'RRaA'
    fsinfo[484:488] = b'rrAa'
    struct.pack_into('<I', fsinfo, 488, 0xFFFFFFFF) # Free clusters
    struct.pack_into('<I', fsinfo, 492, 2) # Next free cluster
    fsinfo[510:512] = b'\x55\xaa'
    disk[512:1024] = fsinfo

    fat_offset = reserved_sectors * 512
    fat_size_bytes = sectors_per_fat * 512
    data_lba = reserved_sectors + (num_fats * sectors_per_fat)

    def set_fat(cluster, val):
        offset = cluster * 4
        for f in range(num_fats):
            fo = fat_offset + (f * fat_size_bytes) + offset
            struct.pack_into('<I', disk, fo, val & 0x0FFFFFFF)

    def get_cluster_offset(cluster):
        lba = data_lba + (cluster - 2) * sectors_per_cluster
        return lba * 512

    # Initialize FAT: cluster 0 (media descriptor), cluster 1 (end marker), cluster 2 (root dir EOC)
    set_fat(0, 0x0FFFFFF8)
    set_fat(1, 0x0FFFFFFF)
    set_fat(2, 0x0FFFFFFF)

    next_cluster = 3

    def add_file_to_dir(dir_cluster, name11, attr, data):
        nonlocal next_cluster
        start_cluster = 0
        if len(data) > 0 or attr == 0x10:
            start_cluster = next_cluster
            # Allocate clusters for data
            bytes_left = len(data)
            curr = start_cluster
            offset = 0
            while True:
                chunk_len = min(bytes_left, sectors_per_cluster * 512)
                c_offset = get_cluster_offset(curr)
                if chunk_len > 0:
                    disk[c_offset:c_offset + chunk_len] = data[offset:offset + chunk_len]
                offset += chunk_len
                bytes_left -= chunk_len
                next_cluster += 1
                if bytes_left <= 0:
                    set_fat(curr, 0x0FFFFFFF)
                    break
                else:
                    set_fat(curr, next_cluster)
                    curr = next_cluster

        # Find empty entry in dir_cluster
        dir_off = get_cluster_offset(dir_cluster)
        for i in range(0, sectors_per_cluster * 512, 32):
            if disk[dir_off + i] == 0x00 or disk[dir_off + i] == 0xE5:
                # Write entry
                entry = bytearray(32)
                entry[0:11] = name11.encode('ascii').ljust(11)
                entry[11] = attr
                struct.pack_into('<H', entry, 20, (start_cluster >> 16) & 0xFFFF)
                struct.pack_into('<H', entry, 26, start_cluster & 0xFFFF)
                struct.pack_into('<I', entry, 28, len(data) if attr != 0x10 else 0)
                disk[dir_off + i:dir_off + i + 32] = entry
                return start_cluster
        raise RuntimeError("Directory full")

    # Add README.TXT
    readme_content = b"Welcome to Greenhouse OS 0.9 (Real Memory & Storage Edition)!\r\nThis file is persistently stored on an ATA/IDE FAT32 drive (C:).\r\n"
    add_file_to_dir(2, "README  TXT", 0x20, readme_content)

    # Add KERNEL.CFG
    cfg_content = b"OS=GREENHOUSE\r\nVERSION=0.9\r\nDRIVE=C\r\nHEAP=DYNAMIC\r\nPAGING=4LEVEL\r\n"
    add_file_to_dir(2, "KERNEL  CFG", 0x20, cfg_content)

    # Add DOCS directory
    docs_cluster = add_file_to_dir(2, "DOCS       ", 0x10, b"")
    # Add . and .. to DOCS directory
    docs_off = get_cluster_offset(docs_cluster)
    dot = bytearray(32)
    dot[0:11] = b'.          '
    dot[11] = 0x10
    struct.pack_into('<H', dot, 20, (docs_cluster >> 16) & 0xFFFF)
    struct.pack_into('<H', dot, 26, docs_cluster & 0xFFFF)
    disk[docs_off:docs_off+32] = dot

    dotdot = bytearray(32)
    dotdot[0:11] = b'..         '
    dotdot[11] = 0x10
    struct.pack_into('<H', dotdot, 20, (2 >> 16) & 0xFFFF)
    struct.pack_into('<H', dotdot, 26, 2 & 0xFFFF)
    disk[docs_off+32:docs_off+64] = dotdot

    # Add INFO.TXT inside DOCS
    info_content = b"Greenhouse OS Storage Architecture:\r\n- VFS Abstraction\r\n- RAMFS (/)\r\n- ATA/IDE PIO Driver\r\n- Microsoft FAT32 Driver\r\n"
    add_file_to_dir(docs_cluster, "INFO    TXT", 0x20, info_content)

    # Add ELF User Binaries from user/bin/
    elf_files = [
        ("HELLO   ELF", "user/bin/HELLO.ELF"),
        ("ECHO    ELF", "user/bin/ECHO.ELF"),
        ("CAT     ELF", "user/bin/CAT.ELF"),
        ("LS      ELF", "user/bin/LS.ELF"),
        ("SLEEP   ELF", "user/bin/SLEEP.ELF"),
        ("PS      ELF", "user/bin/PS.ELF"),
        ("TEST    ELF", "user/bin/TEST.ELF"),
        ("GFX     ELF", "user/bin/GFX.ELF")
    ]

    for name11, filepath in elf_files:
        if os.path.exists(filepath):
            with open(filepath, "rb") as ef:
                elf_data = ef.read()
            add_file_to_dir(2, name11, 0x20, elf_data)
            print(f"  -> Added {name11.strip()} ({len(elf_data)} bytes)")

    with open(filename, "wb") as f:
        f.write(disk)
    print(f"[OK] Created FAT32 disk image: {filename} ({size_mb} MB)")

if __name__ == "__main__":
    make_fat32_disk()
