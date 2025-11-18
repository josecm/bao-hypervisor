/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef VM_SUBARCH_H
#define VM_SUBARCH_H

#include <bao.h>
#include <arch/vfp.h>

struct arch_regs {
    uint32_t elr_hyp;
    uint32_t spsr_hyp;
    uint32_t x[15];
    uint32_t lr_usr;

    uint32_t sp[5];
    uint32_t lr[5];
    uint32_t spsr[5];
    uint32_t fiq_rx[5];

    uint32_t sctlr_el1;
    uint32_t vbar_el1;
    uint64_t tcr_el1;
    uint64_t ttbr0_el1;
    uint64_t ttbr1_el1;
    uint32_t esr_el1;
    uint32_t cpacr_el1;
    uint32_t contextidr_el1;
    uint32_t tpidr_el0;
    uint32_t tpidrro_el0;
    uint32_t tpidr_el1;
    uint32_t csselr_el1;
    uint32_t vmpidr_el2;
    uint32_t cptr_el2;
    uint32_t ifsr32_el2;
    uint64_t mair_el1; // includes mair0 and mair1
    uint64_t far_el1;  // includes ifar and dfar
    uint64_t par_el1;

    struct vfp vfp;
} __attribute__((aligned(16))); // makes size always aligned to 16 to respect stack alignment

struct vcpu;

void vcpu_write_spsr_current_el1_mode(struct vcpu* vcpu, uint32_t spsr_val);
void vcpu_write_lr_current_el1_mode(struct vcpu* vcpu, uint32_t lr_val);

#endif /* VM_SUBARCH_H */
