/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <vm.h>
#include <arch/sysregs.h>
#include <fences.h>
#include <string.h>
#include <config.h>

void vm_arch_init(struct vm* vm, const struct vm_config* vm_config)
{
    vm_arch_profile_init(vm);

    if (vm->master == cpu()->id) {
        vgic_init(vm, &vm_config->platform.arch.gic);
    }
    cpu_sync_and_clear_msgs(&vm->sync);
}

struct vcpu* vm_get_vcpu_by_mpidr(struct vm* vm, unsigned long mpidr)
{
    for (cpuid_t vcpuid = 0; vcpuid < vm->cpu_num; vcpuid++) {
        struct vcpu* vcpu = vm_get_vcpu(vm, vcpuid);
        if ((vcpu->regs.vmpidr_el2 & MPIDR_AFF_MSK) == (mpidr & MPIDR_AFF_MSK)) {
            return vcpu;
        }
    }

    return NULL;
}

static unsigned long vm_cpuid_to_mpidr(struct vm* vm, vcpuid_t cpuid)
{
    if (cpuid > vm->cpu_num) {
        return ~(~MPIDR_RES1 & MPIDR_RES0_MSK); // return an invlid mpidr by
                                                // inverting res bits
    }

    unsigned long mpidr = cpuid | MPIDR_RES1;

    if (vm->cpu_num == 1) {
        mpidr |= MPIDR_U_BIT;
    }

    return mpidr;
}

void vcpu_arch_init(struct vcpu* vcpu, struct vm* vm)
{
    vcpu->regs.vmpidr_el2 = vm_cpuid_to_mpidr(vm, vcpu->id);

    vcpu->arch.psci_ctx.state = vcpu->id == 0 ? ON : OFF;
    vcpu->arch.psci_ctx.lock = SPINLOCK_INITVAL;

    vgic_cpu_init(vcpu);
}

void vcpu_arch_reset(struct vcpu* vcpu, vaddr_t entry)
{
    memset(vcpu->regs.x, 0, sizeof(vcpu->regs.x));

    vcpu_subarch_reset(vcpu);

    vcpu_writepc(vcpu, entry);
    vcpu->regs.sctlr_el1 = SCTLR_RES1;

    vcpu->regs.vbar_el1 = 0;
    vcpu->regs.mair_el1 = 0;
    vcpu->regs.par_el1 = 0;
    vcpu->regs.far_el1 = 0;
    vcpu->regs.esr_el1 = 0;
    vcpu->regs.elr_el1 = 0;
    vcpu->regs.cpacr_el1 = 0;
    vcpu->regs.contextidr_el1 = 0;
    vcpu->regs.tpidr_el0 = 0;
    vcpu->regs.tpidrro_el0 = 0;
    vcpu->regs.tpidr_el1 = 0;
    vcpu->regs.csselr_el1 = 0;

    vcpu->regs.cptr_el2 = 0;

    /**
     *  TODO: ARMv8-A ARM mentions another implementation optional registers that reset to a known
     * value.
     */
}

bool vcpu_arch_is_on(struct vcpu* vcpu)
{
    return vcpu->arch.psci_ctx.state == ON;
}
