/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <vm.h>
#include <arch/sysregs.h>

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

void vcpu_restore_state(struct vcpu *vcpu)
{
    sysreg_vmpidr_el2_write(vcpu->regs.vmpidr_el2);
    sysreg_cptr_el2_write(vcpu->regs.cptr_el2);

    sysreg_sctlr_el1_write(vcpu->regs.sctlr_el1);
    sysreg_sp_el0_write(vcpu->regs.sp_el0);
    sysreg_sp_el1_write(vcpu->regs.sp_el1);
    sysreg_vbar_el1_write(vcpu->regs.vbar_el1);
    sysreg_mair_el1_write(vcpu->regs.mair_el1);
    sysreg_par_el1_write(vcpu->regs.par_el1);
    sysreg_far_el1_write(vcpu->regs.far_el1);
    sysreg_esr_el1_write(vcpu->regs.esr_el1);
    sysreg_elr_el1_write(vcpu->regs.elr_el1);
    sysreg_cpacr_el1_write(vcpu->regs.cpacr_el1);
    sysreg_contextidr_el1_write(vcpu->regs.contextidr_el1);
    sysreg_tpidr_el0_write(vcpu->regs.tpidr_el0);
    sysreg_tpidrro_el0_write(vcpu->regs.tpidrro_el0);
    sysreg_tpidr_el1_write(vcpu->regs.tpidr_el1);
    sysreg_csselr_el1_write(vcpu->regs.csselr_el1);

    vcpu_arch_profile_restore_state(vcpu);

}

void vcpu_save_state(struct vcpu* vcpu)
{
    vcpu->regs.sctlr_el1 = sysreg_sctlr_el1_read();
    vcpu->regs.sp_el0 = sysreg_sp_el0_read();
    vcpu->regs.sp_el1 = sysreg_sp_el1_read();
    vcpu->regs.sctlr_el1 = sysreg_sctlr_el1_read();
    vcpu->regs.vbar_el1 = sysreg_vbar_el1_read();
    vcpu->regs.mair_el1 = sysreg_mair_el1_read();
    vcpu->regs.par_el1 = sysreg_par_el1_read();
    vcpu->regs.far_el1 = sysreg_far_el1_read();
    vcpu->regs.esr_el1 = sysreg_esr_el1_read();
    vcpu->regs.elr_el1 = sysreg_elr_el1_read();
    vcpu->regs.cpacr_el1 = sysreg_cpacr_el1_read();
    vcpu->regs.contextidr_el1 = sysreg_contextidr_el1_read();
    vcpu->regs.tpidr_el0 = sysreg_tpidr_el0_read();
    vcpu->regs.tpidrro_el0 = sysreg_tpidrro_el0_read();
    vcpu->regs.tpidr_el1 = sysreg_tpidr_el1_read();
    vcpu->regs.csselr_el1 = sysreg_csselr_el1_read();

    vcpu_arch_profile_save_state(vcpu);

}
