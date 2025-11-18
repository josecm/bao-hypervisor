/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <vm.h>
#include <arch/sysregs.h>
#include <arch/vfp.h>
#include <arch/vtimer.h>

unsigned long vcpu_readreg(struct vcpu* vcpu, unsigned long reg)
{
    if (reg > 30) {
        return 0;
    }
    return vcpu->regs.x[reg];
}

void vcpu_writereg(struct vcpu* vcpu, unsigned long reg, unsigned long val)
{
    if (reg > 30) {
        return;
    }
    vcpu->regs.x[reg] = val;
}

unsigned long vcpu_readpc(struct vcpu* vcpu)
{
    return vcpu->regs.elr_el2;
}

void vcpu_writepc(struct vcpu* vcpu, unsigned long pc)
{
    vcpu->regs.elr_el2 = pc;
}

void vcpu_subarch_reset(struct vcpu* vcpu)
{
    vcpu->regs.spsr_el2 = SPSR_EL1h | SPSR_F | SPSR_I | SPSR_A | SPSR_D;
}

void vcpu_subarch_restore_state(struct vcpu *vcpu)
{
    sysreg_sp_el0_write(vcpu->regs.sp_el0);
    sysreg_sp_el1_write(vcpu->regs.sp_el1);
    sysreg_elr_el1_write(vcpu->regs.elr_el1);
    sysreg_spsr_el1_write(vcpu->regs.spsr_el1);
}

void vcpu_subarch_save_state(struct vcpu* vcpu)
{
    vcpu->regs.sp_el0 = sysreg_sp_el0_read();
    vcpu->regs.sp_el1 = sysreg_sp_el1_read();
    vcpu->regs.elr_el1 = sysreg_elr_el1_read();
    vcpu->regs.spsr_el1 = sysreg_spsr_el1_read();
}
