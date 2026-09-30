#ifndef KERNEL_CPU_H
#define KERNEL_CPU_H

#include <stdint.h>

typedef struct {
    char vendor[13];
    char brand[49];
    uint32_t max_leaf;
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    uint32_t features_edx;
    uint32_t features_ecx;
    uint32_t ext_features_edx;
    uint32_t ext_features_ecx;
} CPUInfo;

const CPUInfo* get_cpu_info(void);

#endif /* KERNEL_CPU_H */
