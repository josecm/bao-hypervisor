/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef ARCH_PROFILE_VM_H
#define ARCH_PROFILE_VM_H

#include <bao.h>
#include <arch/mpu.h>
#include <plat/platform.h>

#define MAX_NUM_EL1_MPU_REGIONS (32)

struct vcpu_arch_profile {
    struct mpu mpu;
    struct {
        unsigned long prselr;
        unsigned long prbar[MAX_NUM_EL1_MPU_REGIONS];
        unsigned long prlar[MAX_NUM_EL1_MPU_REGIONS];
    } mpu_el1;
};

#endif /* ARCH_PROFILE_CPU_H */
