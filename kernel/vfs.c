#include "vfs.h"
#include "heap.h"

static vfs_drive_t drives[26];

static char vfs_toupper(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return c;
}

static size_t vfs_strlen(const char* s) {
    size_t len = 0;
    while (s && s[len] != '\0') len++;
    return len;
}

static char* vfs_strcpy(char* dest, const char* src) {
    char* orig = dest;
    while ((*dest++ = *src++) != '\0') {}
    return orig;
}

static char* vfs_strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

static int vfs_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static int vfs_strcasecmp(const char* s1, const char* s2) {
    while (*s1 && (vfs_toupper(*s1) == vfs_toupper(*s2))) {
        s1++;
        s2++;
    }
    return (int)vfs_toupper(*(const unsigned char*)s1) - (int)vfs_toupper(*(const unsigned char*)s2);
}

void vfs_init(void) {
    for (int i = 0; i < 26; i++) {
        drives[i].drive_letter = (char)('A' + i);
        drives[i].label[0] = '\0';
        drives[i].fs_type[0] = '\0';
        drives[i].root = NULL;
        drives[i].is_mounted = 0;
    }
}

int vfs_mount(char drive, vfs_node_t* root_node, const char* label, const char* fs_type) {
    char d = vfs_toupper(drive);
    if (d < 'A' || d > 'Z' || !root_node) return -1;
    int idx = d - 'A';

    drives[idx].drive_letter = d;
    drives[idx].root = root_node;
    drives[idx].is_mounted = 1;

    if (label) {
        vfs_strncpy(drives[idx].label, label, 31);
        drives[idx].label[31] = '\0';
    } else {
        drives[idx].label[0] = '\0';
    }

    if (fs_type) {
        vfs_strncpy(drives[idx].fs_type, fs_type, 15);
        drives[idx].fs_type[15] = '\0';
    } else {
        vfs_strncpy(drives[idx].fs_type, "GENERIC", 15);
    }

    return 0;
}

int vfs_unmount(char drive) {
    char d = vfs_toupper(drive);
    if (d < 'A' || d > 'Z') return -1;
    int idx = d - 'A';
    drives[idx].is_mounted = 0;
    drives[idx].root = NULL;
    return 0;
}

vfs_drive_t* vfs_get_drive(char drive) {
    char d = vfs_toupper(drive);
    if (d < 'A' || d > 'Z') return NULL;
    int idx = d - 'A';
    if (drives[idx].is_mounted) return &drives[idx];
    return NULL;
}

vfs_node_t* vfs_get_drive_root(char drive) {
    vfs_drive_t* drv = vfs_get_drive(drive);
    if (drv) return drv->root;
    return NULL;
}

int vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !node->read || !buffer) return -1;
    return node->read(node, offset, size, buffer);
}

int vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (!node || !node->write || !buffer) return -1;
    return node->write(node, offset, size, buffer);
}

int vfs_readdir(vfs_node_t* node, uint32_t index, vfs_dirent_t* dirent) {
    if (!node || !node->readdir || !dirent) return -1;
    return node->readdir(node, index, dirent);
}

vfs_node_t* vfs_finddir(vfs_node_t* node, const char* name) {
    if (!node || !node->finddir || !name) return NULL;
    return node->finddir(node, name);
}

/* Parse path string and resolve to target VFS node */
vfs_node_t* vfs_resolve_path(const char* current_path, const char* path) {
    if (!path || *path == '\0') {
        if (!current_path || *current_path == '\0') return NULL;
        return vfs_resolve_path(NULL, current_path);
    }

    char default_drive = 'C';
    if (current_path && current_path[0] >= 'A' && current_path[0] <= 'Z' && current_path[1] == ':') {
        default_drive = current_path[0];
    } else if (current_path && current_path[0] >= 'a' && current_path[0] <= 'z' && current_path[1] == ':') {
        default_drive = vfs_toupper(current_path[0]);
    }

    const char* p = path;
    char target_drive = default_drive;

    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':') {
        target_drive = vfs_toupper(p[0]);
        p += 2;
    }

    vfs_node_t* curr = vfs_get_drive_root(target_drive);
    if (!curr) return NULL;

    /* If relative path without leading slash, start from current_path if on same drive */
    if (*p != '\\' && *p != '/' && current_path && vfs_toupper(current_path[0]) == target_drive) {
        /* Start from current working directory */
        curr = vfs_resolve_path(NULL, current_path);
        if (!curr) curr = vfs_get_drive_root(target_drive);
    }

    while (*p == '\\' || *p == '/') p++;

    char token[64];
    while (*p != '\0') {
        while (*p == '\\' || *p == '/') p++;
        if (*p == '\0') break;

        int len = 0;
        while (*p != '\0' && *p != '\\' && *p != '/' && len < 63) {
            token[len++] = *p++;
        }
        token[len] = '\0';

        if (vfs_strcmp(token, ".") == 0) {
            continue;
        } else if (vfs_strcmp(token, "..") == 0) {
            vfs_node_t* parent = vfs_finddir(curr, "..");
            if (parent) {
                curr = parent;
            }
        } else {
            vfs_node_t* child = vfs_finddir(curr, token);
            if (!child) {
                return NULL;
            }
            curr = child;
        }
    }

    return curr;
}

int vfs_resolve_parent_and_leaf(const char* current_path, const char* path, vfs_node_t** out_parent, char* out_leaf) {
    if (!path || *path == '\0' || !out_parent || !out_leaf) return -1;

    char default_drive = 'C';
    if (current_path && current_path[0] >= 'A' && current_path[0] <= 'Z' && current_path[1] == ':') {
        default_drive = current_path[0];
    } else if (current_path && current_path[0] >= 'a' && current_path[0] <= 'z' && current_path[1] == ':') {
        default_drive = vfs_toupper(current_path[0]);
    }

    const char* p = path;
    char target_drive = default_drive;

    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':') {
        target_drive = vfs_toupper(p[0]);
        p += 2;
    }

    vfs_node_t* curr = vfs_get_drive_root(target_drive);
    if (!curr) return -1;

    if (*p != '\\' && *p != '/' && current_path && vfs_toupper(current_path[0]) == target_drive) {
        curr = vfs_resolve_path(NULL, current_path);
        if (!curr) curr = vfs_get_drive_root(target_drive);
    }

    while (*p == '\\' || *p == '/') p++;

    char token[64];
    while (*p != '\0') {
        while (*p == '\\' || *p == '/') p++;
        if (*p == '\0') break;

        int len = 0;
        while (*p != '\0' && *p != '\\' && *p != '/' && len < 63) {
            token[len++] = *p++;
        }
        token[len] = '\0';

        const char* peek = p;
        while (*peek == '\\' || *peek == '/') peek++;
        int is_last = (*peek == '\0');

        if (is_last) {
            if (vfs_strcmp(token, ".") == 0 || vfs_strcmp(token, "..") == 0) {
                return -1;
            }
            *out_parent = curr;
            vfs_strncpy(out_leaf, token, 63);
            out_leaf[63] = '\0';
            return 0;
        } else {
            if (vfs_strcmp(token, ".") == 0) {
                continue;
            } else if (vfs_strcmp(token, "..") == 0) {
                vfs_node_t* parent = vfs_finddir(curr, "..");
                if (parent) curr = parent;
            } else {
                vfs_node_t* child = vfs_finddir(curr, token);
                if (!child || !(child->flags & VFS_DIRECTORY)) {
                    return -1;
                }
                curr = child;
            }
        }
    }

    return -1;
}

int vfs_mkdir_path(const char* current_path, const char* path) {
    vfs_node_t* parent = NULL;
    char leaf[64];
    if (vfs_resolve_parent_and_leaf(current_path, path, &parent, leaf) != 0) {
        return -1;
    }
    if (!parent || !parent->mkdir) return -1;
    return parent->mkdir(parent, leaf);
}

int vfs_create_file(const char* current_path, const char* path) {
    vfs_node_t* parent = NULL;
    char leaf[64];
    if (vfs_resolve_parent_and_leaf(current_path, path, &parent, leaf) != 0) {
        return -1;
    }
    if (!parent || !parent->create) return -1;
    return parent->create(parent, leaf, 0);
}

int vfs_delete_path(const char* current_path, const char* path) {
    vfs_node_t* parent = NULL;
    char leaf[64];
    if (vfs_resolve_parent_and_leaf(current_path, path, &parent, leaf) != 0) {
        return -1;
    }
    if (!parent || !parent->delete) return -1;
    return parent->delete(parent, leaf);
}

int vfs_rename_path(const char* current_path, const char* old_path, const char* new_path) {
    vfs_node_t* parent = NULL;
    char old_leaf[64];
    if (vfs_resolve_parent_and_leaf(current_path, old_path, &parent, old_leaf) != 0) {
        return -1;
    }
    if (!parent || !parent->rename) return -1;
    return parent->rename(parent, old_leaf, new_path);
}
