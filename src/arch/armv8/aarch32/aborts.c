/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <arch/aborts.h>
#include <arch/sysregs.h>
#include <cpu.h>
#include <vm.h>

#define ABORTS_IS_INST  (true)
#define ABORTS_NOT_INST (false)

void internal_abort_handler(unsigned long gprs[])
{
    for (ssize_t i = 14; i >= 0; i--) {
        console_printk("x%d:\t\t0x%0lx\n", i, gprs[14 - i]);
    }
    console_printk("ESR:\t0x%0lx\n", sysreg_esr_el2_read());
    console_printk("ELR:\t0x%0lx\n", sysreg_elr_el2_read());
    console_printk("FAR:\t0x%0lx\n", sysreg_far_el2_read());
    ERROR("cpu%d internal hypervisor abort - PANIC\n", cpu()->id);
}

static vaddr_t aborts_vcpu_vector_addr(uint32_t mode, bool inst_abort)
{
    vaddr_t vec_base_addr = sysreg_vbar_el1_read(); 
    vaddr_t vec_offset;

    switch (mode & SPSR_M_MSK) {
        case SPSR_ABT:
            vec_offset = inst_abort ? 0x0C : 0x10;
            break;
        case SPSR_UND:
            vec_offset = 0x4;
            break;
        default:
            // If later we need to inject exception at other mode we need to add the other offsets.
            ERROR("Trying to get vector address for injecting vcpu exception at unsuported privilege level.");
    }

    return vec_base_addr + vec_offset;
}

static void aborts_vcpu_set_exception_cpsr(struct vcpu* vcpu, uint32_t spsr_mode) {
    static const uint32_t CPSR_PRESERVED_BITS = ~(SPSR_M_MSK | SPSR_IT_MSK | SPSR_J | SPSR_E | SPSR_T);
    uint32_t new_cpsr = vcpu->regs.spsr_hyp & CPSR_PRESERVED_BITS;
    new_cpsr |= SPSR_I;
    new_cpsr |= (spsr_mode & SPSR_M_MSK);

    if ((spsr_mode & SPSR_M_MSK) == SPSR_ABT) {
        new_cpsr |= SPSR_A;
    }

    if ((sysreg_sctlr_el1_read() & SCTLR_TE) != 0) {
        new_cpsr |= SPSR_T;
    }

    if ((sysreg_sctlr_el1_read() & SCTLR_EE) != 0) {
        new_cpsr |= SPSR_E;
    }

    vcpu->regs.spsr_hyp = new_cpsr;
}

static unsigned long vcpu_exception_lr(struct vcpu* vcpu, uint32_t next_mode, bool inst_abort)
{
    unsigned long lr_offset = 0;

    if (next_mode == SPSR_UND) {
        bool vcpu_in_thumb = ((vcpu->regs.spsr_hyp & SPSR_T) != 0);
        if (vcpu_in_thumb) {
            lr_offset = 2;
        } else {
            lr_offset = 4;
        }
    } else {
        /**
         * Note we are only supporting to modes for exception injection UND and ABT
         */
        if (inst_abort) {
            lr_offset = 4;
        } else {
            lr_offset = 8;
        }
    }

    return vcpu_readpc(vcpu) + lr_offset;
}

static inline bool aborts_vcpu_exception_finish(uint32_t mode, bool inst_abort)
{
    struct vcpu* vcpu = cpu()->vcpu;
    uint32_t spsr = vcpu->regs.spsr_hyp;

    vcpu_write_lr_current_el1_mode(vcpu, vcpu_exception_lr(vcpu, mode, inst_abort));
    aborts_vcpu_set_exception_cpsr(vcpu, mode); // updates cpsr, switching mode
    vcpu_write_spsr_current_el1_mode(vcpu, spsr); // sets the target mode spsr to the one the hypervisor got
    vcpu_writepc(vcpu, aborts_vcpu_vector_addr(mode, inst_abort));

    return true;
}

static bool aborts_vcpu_inject_mem_abort(bool inst_abort, unsigned long iss, unsigned long far, unsigned long il, unsigned long ec)
{
    UNUSED_ARG(il);
    UNUSED_ARG(ec);

    unsigned long fsr = 0;

    bool guest_using_lpae = false;
    if (DEFINED(MEM_PROT_MMU)) {
        guest_using_lpae = ((sysreg_ttbcr_read() & TTBCR_EAE) != 0);

        if (guest_using_lpae) {
            fsr |= FSR_LPAE_BIT;
        }
    } 

    fsr |= FSR_ExT_BIT;

    if (DEFINED(MEM_PROT_MMU) && ((iss & ESR_ISS_DA_S1PTW_BIT) != 0)) {
        // If the fault was due to a stage 1 walk, we default to translation level 1 fault. 
        // Figuring out the true level of the stage 1 walk that generated the fault would 
        // requiring manually walking the guest's tables which is a non-trivial operation
        // mainly because bao does not have the guest's address space mapped an because
        // this would require to stop all other vcpus to make sure they do not modify the 
        // page table while bao is walking them creating possible attack vectors.
        fsr |= guest_using_lpae ? ESR_ISS_DA_DSFC_SYNC_EXT_PTW_LVL1 : FSR_SYNC_EXT_PTW_LVL1;
    } else {
        if ((iss & ESR_ISS_DA_FnV_BIT) != 0) {
            fsr |= FSR_FnV_BIT;
            far = 0;
        }
        fsr |= (DEFINED(MEM_PROT_MPU) || guest_using_lpae) ? ESR_ISS_DA_DFSC_SYNC_EXT : FSR_SYNC_EXT;
    }

    if (inst_abort) {
        sysreg_ifsr_write(fsr);
        sysreg_ifar_write(far);
    } else {
        if ((iss & ESR_ISS_DA_WnR_BIT) != 0) {
            fsr |= FSR_WnR_BIT;
        }
        sysreg_dfsr_write(fsr);
        sysreg_dfar_write(far);
    }

    return aborts_vcpu_exception_finish(SPSR_ABT, inst_abort);
}

bool aborts_vcpu_inject_data_abort(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec)
{
    return aborts_vcpu_inject_mem_abort(ABORTS_NOT_INST, iss, far, il, ec);
}

bool aborts_vcpu_inject_inst_abort(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec)
{
    return aborts_vcpu_inject_mem_abort(ABORTS_IS_INST, iss, far, il, ec);
}

bool aborts_vcpu_inject_undef_abort(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec)
{
    UNUSED_ARG(iss);
    UNUSED_ARG(far);
    UNUSED_ARG(il);
    UNUSED_ARG(ec);

    return aborts_vcpu_exception_finish(SPSR_UND, ABORTS_NOT_INST);
}
