/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef ARCH_PROFILE_CPU_H
#define ARCH_PROFILE_CPU_H

#include <bao.h>
#include <arch/sysregs.h>
#include <arch/mpu.h>
#include <bitmap.h>
#include <list.h>
#include <mem.h>
#include <list.h>

struct cpu_arch_profile {
    struct {
        size_t entry_allocation_count[PLAT_NUM_EL2_MPU_REGIONS];
        unsigned long locked_entries;
        bool* resident[PLAT_NUM_EL2_MPU_REGIONS];
        struct mpu* active_guest_mpu;
    } mpu_mngmnt;

    struct mpu mpu;
};

static inline struct cpu* cpu(void)
{
    return (struct cpu*)sysreg_tpidr_el2_read();
}

#endif /* ARCH_PROFILE_CPU_H */
