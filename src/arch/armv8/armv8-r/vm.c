/**
 * Bao, a Lightweight Static Partitioning Hypervisor
 *
 * Copyright (c) Bao Project (www.bao-project.org), 2019-
 *
 * Authors:
 *      Sandro Pinto <sandro@bao-project.org>
 *      Afonso Santos <afomms@gmail.com>
 *
 * Bao is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 2 as published by the Free
 * Software Foundation, with a special exception exempting guest code from such
 * license. See the COPYING file in the top-level directory for details.
 *
 */

#include <vm.h>
#include <arch/sysregs.h>

void vcpu_arch_profile_init(struct vcpu* vcpu, struct vm* vm) {
    // TODO: enable mpu region of the vcpu (cpu struct)
}

void vcpu_arch_profile_reset(struct vcpu* vcpu) {
    vcpu->regs.spsr_hyp = SPSR_SVC | SPSR_F | SPSR_I | SPSR_A;
}

bool vcpu_arch_profile_on(struct vcpu* vcpu) {
    // TODO: Store "ON" state on the struct that will manage the power facility 
    return true;
}
