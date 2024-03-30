/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef VM_SUBARCH_H
#define VM_SUBARCH_H

#include <bao.h>

struct arch_regs {
    uint64_t x[31];
    uint64_t elr_el2;
    uint64_t spsr_el2;

    uint64_t sp_el0;
    uint64_t sp_el1;
    uint64_t sctlr_el1;
    uint64_t vbar_el1;
    uint64_t tcr_el1;
    uint64_t ttbr0_el1;
    uint64_t ttbr1_el1;
    uint64_t mair_el1;
    uint64_t par_el1;
    uint64_t far_el1;
    uint64_t esr_el1;
    uint64_t elr_el1;
    uint64_t cpacr_el1;
    uint64_t contextidr_el1;
    uint64_t tpidr_el0;
    uint64_t tpidrro_el0;
    uint64_t tpidr_el1;
    uint64_t csselr_el1;
    uint64_t vmpidr_el2;
    uint64_t cptr_el2;

} __attribute__((aligned(16))); // makes size always aligned to 16 to respect stack alignment

#endif                          /* VM_SUBARCH_H */
