#include "ramfs.h"
#include "heap.h"

typedef struct ramfs_node {
    char name[64];
    uint8_t is_dir;
    uint32_t size;
    uint32_t capacity;
    uint8_t* data;
    uint32_t inode;

    struct ramfs_node* parent;
    struct ramfs_node* first_child;
    struct ramfs_node* next_sibling;

    vfs_node_t vfs_node;
} ramfs_node_t;

static uint32_t ramfs_next_inode = 1;

static char r_toupper(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return c;
}

static size_t r_strlen(const char* s) {
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}

static int r_strcasecmp(const char* s1, const char* s2) {
    while (*s1 && (r_toupper(*s1) == r_toupper(*s2))) {
        s1++;
        s2++;
    }
    return (int)r_toupper(*(const unsigned char*)s1) - (int)r_toupper(*(const unsigned char*)s2);
}

static char* r_strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

static void* r_memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < count; i++) d[i] = s[i];
    return dest;
}

static void* r_memset(void* dest, int val, size_t count) {
    uint8_t* ptr = (uint8_t*)dest;
    for (size_t i = 0; i < count; i++) ptr[i] = (uint8_t)val;
    return dest;
}

static int ramfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
static int ramfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
static int ramfs_readdir(vfs_node_t* node, uint32_t index, vfs_dirent_t* dirent);
static vfs_node_t* ramfs_finddir(vfs_node_t* node, const char* name);
static int ramfs_mkdir(vfs_node_t* parent, const char* name);
static int ramfs_create(vfs_node_t* parent, const char* name, uint32_t flags);
static int ramfs_delete(vfs_node_t* parent, const char* name);
static int ramfs_rename(vfs_node_t* parent, const char* old_name, const char* new_name);

static ramfs_node_t* ramfs_alloc_node(const char* name, uint8_t is_dir, ramfs_node_t* parent) {
    ramfs_node_t* node = (ramfs_node_t*)kmalloc(sizeof(ramfs_node_t));
    if (!node) return NULL;
    r_memset(node, 0, sizeof(ramfs_node_t));

    r_strncpy(node->name, name, 63);
    node->is_dir = is_dir;
    node->parent = parent;
    node->inode = ramfs_next_inode++;

    r_strncpy(node->vfs_node.name, name, 63);
    node->vfs_node.flags = is_dir ? VFS_DIRECTORY : VFS_FILE;
    node->vfs_node.size = 0;
    node->vfs_node.inode = node->inode;
    node->vfs_node.priv_data = node;

    node->vfs_node.read = ramfs_read;
    node->vfs_node.write = ramfs_write;
    node->vfs_node.readdir = ramfs_readdir;
    node->vfs_node.finddir = ramfs_finddir;
    node->vfs_node.mkdir = ramfs_mkdir;
    node->vfs_node.create = ramfs_create;
    node->vfs_node.delete = ramfs_delete;
    node->vfs_node.rename = ramfs_rename;

    return node;
}

static int ramfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !buffer) return -1;
    ramfs_node_t* rnode = (ramfs_node_t*)node->priv_data;
    if (!rnode || rnode->is_dir) return -1;

    if (offset >= rnode->size) return 0;
    uint32_t bytes_to_read = size;
    if (offset + bytes_to_read > rnode->size) {
        bytes_to_read = rnode->size - offset;
    }

    if (rnode->data && bytes_to_read > 0) {
        r_memcpy(buffer, rnode->data + offset, bytes_to_read);
    }
    return (int)bytes_to_read;
}

static int ramfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (!node || !buffer) return -1;
    ramfs_node_t* rnode = (ramfs_node_t*)node->priv_data;
    if (!rnode || rnode->is_dir) return -1;

    uint32_t required_capacity = offset + size;
    if (required_capacity > rnode->capacity) {
        uint32_t new_cap = (required_capacity < 64) ? 64 : (required_capacity * 2);
        uint8_t* new_data = (uint8_t*)krealloc(rnode->data, new_cap);
        if (!new_data) return -1;
        rnode->data = new_data;
        rnode->capacity = new_cap;
    }

    r_memcpy(rnode->data + offset, buffer, size);
    if (offset + size > rnode->size) {
        rnode->size = offset + size;
        node->size = rnode->size;
    }
    return (int)size;
}

static int ramfs_readdir(vfs_node_t* node, uint32_t index, vfs_dirent_t* dirent) {
    if (!node || !dirent) return -1;
    ramfs_node_t* rnode = (ramfs_node_t*)node->priv_data;
    if (!rnode || !rnode->is_dir) return -1;

    ramfs_node_t* curr = rnode->first_child;
    uint32_t idx = 0;
    while (curr) {
        if (idx == index) {
            r_strncpy(dirent->name, curr->name, 63);
            dirent->size = curr->size;
            dirent->is_dir = curr->is_dir;
            dirent->inode = curr->inode;
            return 0;
        }
        idx++;
        curr = curr->next_sibling;
    }
    return -1;
}

static vfs_node_t* ramfs_finddir(vfs_node_t* node, const char* name) {
    if (!node || !name) return NULL;
    ramfs_node_t* rnode = (ramfs_node_t*)node->priv_data;
    if (!rnode || !rnode->is_dir) return NULL;

    if (r_strcasecmp(name, ".") == 0) {
        return node;
    }

    if (r_strcasecmp(name, "..") == 0) {
        if (rnode->parent) {
            return &rnode->parent->vfs_node;
        }
        return node;
    }

    ramfs_node_t* curr = rnode->first_child;
    while (curr) {
        if (r_strcasecmp(curr->name, name) == 0) {
            return &curr->vfs_node;
        }
        curr = curr->next_sibling;
    }
    return NULL;
}

static int ramfs_mkdir(vfs_node_t* parent, const char* name) {
    if (!parent || !name || *name == '\0') return -1;
    ramfs_node_t* pnode = (ramfs_node_t*)parent->priv_data;
    if (!pnode || !pnode->is_dir) return -1;

    /* Check duplicate */
    if (ramfs_finddir(parent, name) != NULL) return -1;

    ramfs_node_t* new_dir = ramfs_alloc_node(name, 1, pnode);
    if (!new_dir) return -1;

    /* Append to children */
    if (!pnode->first_child) {
        pnode->first_child = new_dir;
    } else {
        ramfs_node_t* curr = pnode->first_child;
        while (curr->next_sibling) curr = curr->next_sibling;
        curr->next_sibling = new_dir;
    }

    return 0;
}

static int ramfs_create(vfs_node_t* parent, const char* name, uint32_t flags) {
    (void)flags;
    if (!parent || !name || *name == '\0') return -1;
    ramfs_node_t* pnode = (ramfs_node_t*)parent->priv_data;
    if (!pnode || !pnode->is_dir) return -1;

    vfs_node_t* existing = ramfs_finddir(parent, name);
    if (existing) {
        /* Truncate file */
        ramfs_node_t* enode = (ramfs_node_t*)existing->priv_data;
        enode->size = 0;
        existing->size = 0;
        return 0;
    }

    ramfs_node_t* new_file = ramfs_alloc_node(name, 0, pnode);
    if (!new_file) return -1;

    if (!pnode->first_child) {
        pnode->first_child = new_file;
    } else {
        ramfs_node_t* curr = pnode->first_child;
        while (curr->next_sibling) curr = curr->next_sibling;
        curr->next_sibling = new_file;
    }

    return 0;
}

static int ramfs_delete(vfs_node_t* parent, const char* name) {
    if (!parent || !name) return -1;
    ramfs_node_t* pnode = (ramfs_node_t*)parent->priv_data;
    if (!pnode || !pnode->is_dir) return -1;

    ramfs_node_t* prev = NULL;
    ramfs_node_t* curr = pnode->first_child;

    while (curr) {
        if (r_strcasecmp(curr->name, name) == 0) {
            if (curr->is_dir && curr->first_child) {
                return -1; /* Directory not empty */
            }

            if (prev) {
                prev->next_sibling = curr->next_sibling;
            } else {
                pnode->first_child = curr->next_sibling;
            }

            if (curr->data) {
                kfree(curr->data);
            }
            kfree(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next_sibling;
    }
    return -1;
}

static int ramfs_rename(vfs_node_t* parent, const char* old_name, const char* new_name) {
    if (!parent || !old_name || !new_name) return -1;
    vfs_node_t* target = ramfs_finddir(parent, old_name);
    if (!target) return -1;

    if (ramfs_finddir(parent, new_name) != NULL) return -1; /* Destination already exists */

    ramfs_node_t* rnode = (ramfs_node_t*)target->priv_data;
    r_strncpy(rnode->name, new_name, 63);
    r_strncpy(target->name, new_name, 63);
    return 0;
}

static void ramfs_populate_file(vfs_node_t* dir, const char* name, const char* content) {
    ramfs_create(dir, name, 0);
    vfs_node_t* f = ramfs_finddir(dir, name);
    if (f) {
        ramfs_write(f, 0, (uint32_t)r_strlen(content), (const uint8_t*)content);
    }
}

vfs_node_t* ramfs_create_root(void) {
    ramfs_node_t* root = ramfs_alloc_node("", 1, NULL);
    if (!root) return NULL;

    /* Pre-populate directories */
    ramfs_mkdir(&root->vfs_node, "GREENHOUSE");
    ramfs_mkdir(&root->vfs_node, "SYSTEM");
    ramfs_mkdir(&root->vfs_node, "USERS");
    ramfs_mkdir(&root->vfs_node, "BERRY");
    ramfs_mkdir(&root->vfs_node, "WINDOWS");

    vfs_node_t* gh_dir = ramfs_finddir(&root->vfs_node, "GREENHOUSE");
    vfs_node_t* sys_dir = ramfs_finddir(&root->vfs_node, "SYSTEM");
    vfs_node_t* users_dir = ramfs_finddir(&root->vfs_node, "USERS");
    vfs_node_t* berry_dir = ramfs_finddir(&root->vfs_node, "BERRY");

    if (users_dir) {
        ramfs_mkdir(users_dir, "VIVAAN");
        vfs_node_t* vivaan_dir = ramfs_finddir(users_dir, "VIVAAN");
        if (vivaan_dir) {
            ramfs_populate_file(vivaan_dir, "NOTES.TXT",
                "Vivaan's Developer Notes:\n"
                "- Phase 2 Real Memory (PMM + VMM + Heap) complete!\n"
                "- Phase 3 Real Storage (VFS + RAMFS + FAT32 + ATA) complete!\n"
                "- Dynamic kmalloc/kfree throughout filesystem & kernel.\n");
        }
    }

    if (gh_dir) {
        ramfs_populate_file(gh_dir, "README.TXT",
            "=====================================================\n"
            " Greenhouse OS 0.9 (x86_64 Edition)\n"
            " Built from scratch by Vivaan, Founder of Berry-Tech\n"
            "=====================================================\n\n"
            "Welcome to Greenhouse OS 0.9!\n"
            "Features:\n"
            "- Physical Page Frame Allocator (4 KiB PMM)\n"
            "- Virtual Memory 4-Level Paging (VMM)\n"
            "- Kernel Heap Allocator (kmalloc / kfree)\n"
            "- Virtual Filesystem (VFS) with Drive Letters (C:, R:)\n"
            "- Persistent FAT32 Disk Driver (ATA PIO)\n"
            "- Dynamic RAM Filesystem (RAMFS)\n"
            "- Berry System Companion & Command History\n\n"
            "Type HELP for all available commands.\n"
            "Type BERRY to explore Berry's features.\n");
    }

    if (berry_dir) {
        ramfs_populate_file(berry_dir, "HELLO.TXT",
            "Hello from Berry!\n\n"
            "\"If it runs code, you can make it your own.\"\n"
            "Greenhouse OS is designed to be lean, fast, and empowering.\n");
    }

    if (sys_dir) {
        ramfs_populate_file(sys_dir, "CONFIG.SYS",
            "FILES=128\n"
            "BUFFERS=64\n"
            "SHELL=BERRY_SH.EXE\n"
            "ARCH=X86_64\n"
            "MODE=LONG_MODE\n"
            "VERSION=0.9.0\n");

        ramfs_populate_file(sys_dir, "OS.VER",
            "Greenhouse OS Version 0.9 (x86_64 Memory + Storage Edition)\n");
    }

    return &root->vfs_node;
}
