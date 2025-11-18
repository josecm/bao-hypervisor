/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <arch/aborts.h>
#include <arch/sysregs.h>
#include <cpu.h>
#include <vm.h>
#include <config.h>

#define ABORTS_IS_INST  (true)
#define ABORTS_NOT_INST (false)

/**
 * This codes assumes that an abort handle is always called for the currently 
 * running cpu.
 */

void internal_abort_handler(unsigned long gprs[])
{
    for (size_t i = 0; i < 31; i++) {
        console_printk("x%d:\t\t0x%0lx\n", i, gprs[i]);
    }
    console_printk("SP:\t\t0x%0lx\n", gprs[31]);
    console_printk("ESR:\t0x%0lx\n", sysreg_esr_el2_read());
    console_printk("ELR:\t0x%0lx\n", sysreg_elr_el2_read());
    console_printk("FAR:\t0x%0lx\n", sysreg_far_el2_read());
    ERROR("cpu%d internal hypervisor abort - PANIC\n", cpu()->id);
}

static inline bool aborts_vcpu_at_el1(void)
{
    struct vcpu *vcpu = cpu()->vcpu;

    return ((vcpu->regs.spsr_el2 & SPSR_EL_MSK) == SPSR_EL1h) ||
        ((vcpu->regs.spsr_el2 & SPSR_EL_MSK) == SPSR_EL1t);
}

static inline bool aborts_vcpu_at_el1_thread(void)
{
    struct vcpu *vcpu = cpu()->vcpu;
    return (vcpu->regs.spsr_el2 & SPSR_EL_MSK) == SPSR_EL1t;
}

static inline bool aborts_vcpu_el0_in_aarch32(void)
{
    struct vcpu *vcpu = cpu()->vcpu;
    return (vcpu->regs.spsr_el2 & SPSR_AARCH32) != 0;
}

static vaddr_t aborts_vcpu_vector_addr(void)
{
    vaddr_t vec_offset;
    if (!aborts_vcpu_at_el1()) {
        if (aborts_vcpu_el0_in_aarch32()) {
            vec_offset = 0x600;
        } else {
            vec_offset = 0x400;
        }
    } else {
        if (aborts_vcpu_at_el1_thread()) {
            vec_offset = 0x0;
        } else {
            vec_offset = 0x200;
        }
    }

    return sysreg_vbar_el1_read() + vec_offset;
}

static bool aborts_vcpu_inject_sync_abort(unsigned long esr_el1, unsigned long far_el1, unsigned long il)
{
    struct vcpu *vcpu = cpu()->vcpu;

    /*
     * let's compute the vector's address before updating spsr_el2 (guest's pstate), since it depends on it
     */
    vaddr_t vec_addr = aborts_vcpu_vector_addr(); 

    if (il != 0) {
        esr_el1 |= ESR_IL_BIT;
    }

    sysreg_esr_el1_write(esr_el1);
    sysreg_far_el1_write(far_el1);
    sysreg_spsr_el1_write(vcpu->regs.spsr_el2);
    vcpu->regs.spsr_el2 = SPSR_EL1h | SPSR_A | SPSR_I | SPSR_F | SPSR_D;
    sysreg_elr_el1_write(vcpu_readpc(vcpu));
    vcpu_writepc(vcpu, vec_addr);

    return true;
}

static bool aborts_vcpu_inject_mem_abort(bool inst_abort, unsigned long iss, unsigned long far, unsigned long il, unsigned long ec)
{
    UNUSED_ARG(ec);

    unsigned long esr_el1 = 0;

    if (inst_abort) {
        esr_el1 = aborts_vcpu_at_el1() ? ESR_EC_IASEL : ESR_EC_IALEL; 
    } else {
        esr_el1 =  aborts_vcpu_at_el1() ? ESR_EC_DASEL : ESR_EC_DALEL;
    }

    /**
     * Note that ISS encoding for both data and instruction aborts is the same, hence we use macros for DA only.
     */

    esr_el1 |= ESR_ISS_DA_EA_BIT;
    esr_el1 |= (iss & ESR_ISS_DA_FnV_BIT);

    if (!inst_abort) {
        esr_el1 |= iss & ESR_ISS_DA_WnR_BIT;
    }

    bool guest_using_mmu = DEFINED(MEM_PROT_MMU) || cpu()->vcpu->vm->config->platform.mmu;
    if (guest_using_mmu && ((iss & ESR_ISS_DA_S1PTW_BIT) != 0)) {
        // If the fault was due to a stage 1 walk, we default to translation level 0 fault. 
        // Figuring out the true level of the stage 1 walk that generated the fault would 
        // requiring manually walking the guest's tables which is a non-trivial operation
        // mainly because bao does not have the guest's address space mapped an because
        // this would require to stop all other vcpus to make sure they do not modify the 
        // page table while bao is walking them creating possible attack vectors.
        esr_el1 |= ESR_ISS_DA_DSFC_SYNC_EXT_PTW_LVL0;
    } else {
        esr_el1 |= ESR_ISS_DA_DFSC_SYNC_EXT;
    }

    return aborts_vcpu_inject_sync_abort(esr_el1, far, il);
}

bool aborts_vcpu_inject_data_abort(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec)
{
    return aborts_vcpu_inject_mem_abort(ABORTS_NOT_INST, iss, far, il, ec);
}

bool aborts_vcpu_inject_inst_abort(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec)
{
    return  aborts_vcpu_inject_mem_abort(ABORTS_IS_INST, iss, far, il, ec);
}

bool aborts_vcpu_inject_undef_abort(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec)
{
    UNUSED_ARG(iss);
    UNUSED_ARG(far);
    UNUSED_ARG(ec);

    uint64_t esr_el1 = ESR_EC_UNKWN;
    return aborts_vcpu_inject_sync_abort(esr_el1, 0, il);
}
