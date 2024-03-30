/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <vm.h>
#include <arch/sysregs.h>
#include <arch/fences.h>
#include <tlb.h>

void vm_arch_profile_init(struct vm* vm)
{
    paddr_t root_pt_pa;
    mem_translate(&cpu()->as, (vaddr_t)vm->as.pt.root, &root_pt_pa);
    vcpu->vm->arch.vttbr_el2 = (((uint64_t)vm->id << VTTBR_VMID_OFF) & VTTBR_VMID_MSK) |
         (root_pt_pa & ~VTTBR_VMID_MSK);

    ISB(); // make sure vmid is commited befor tlbi
    tlb_vm_inv_all(vm->id);

    vcpu->regs.ttbr0_el1 = 0;
    vcpu->regs.ttbr1_el1 = 0;
    vcpu->regs.tcr_el1 = 0;

}

void vcpu_arch_profile_save_state(struct vcpu* vcpu)
{
    vcpu->regs.tcr_el1 = sysreg_tcr_el1_read();
    vcpu->regs.ttbr0_el1 = sysreg_ttbr0_el1_read();
    vcpu->regs.ttbr1_el1 = sysreg_ttbr1_el1_read();
}

void vcpu_arch_profile_restore_state(struct vcpu* vcpu)
{
    sysreg_vttbr_el2_write(vcpu->vm->arch.vttbr_el2);
    sysreg_tcr_el1_write(vcpu->regs.tcr_el1);
    sysreg_ttbr0_el1_write(vcpu->regs.ttbr0_el1);
    sysreg_ttbr1_el1_write(vcpu->regs.ttbr1_el1);
}
