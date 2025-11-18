/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef __ARCH_MPU_H__
#define __ARCH_MPU_H__

#include <bao.h>
#include <plat/platform.h>

#define MPU_NUM_ENTRIES (PLAT_NUM_EL2_MPU_REGIONS)

struct mpu {
    bool active;
    bool resident;
    mpid_t first_entry;

    struct mpu_entry {
        unsigned long prbar;
        unsigned long prlar;
    } entry[MPU_NUM_ENTRIES];

    unsigned long prenr;
    unsigned long locked;
};

void mpu_struct_init(struct mpu* mpu);
void mpu_restore(struct mpu* mpu);

#endif /* __ARCH_MPU_H__ */
