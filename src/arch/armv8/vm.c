/**
 * Bao, a Lightweight Static Partitioning Hypervisor
 *
 * Copyright (c) Bao Project (www.bao-project.org), 2019-
 *
 * Authors:
 *      Jose Martins <jose.martins@bao-project.org>
 *
 * Bao is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 2 as published by the Free
 * Software Foundation, with a special exception exempting guest code from such
 * license. See the COPYING file in the top-level directory for details.
 *
 */

#include <vm.h>

#include <vmm.h>
#include <page_table.h>
#include <arch/sysregs.h>
#include <fences.h>
#include <arch/tlb.h>
#include <string.h>

void vm_arch_init(vm_t* vm, const vm_config_t* config)
{
    if (vm->master == cpu.id) {
        vgic_init(vm, &config->platform.arch.gic);
    }
}

vcpu_t* vm_get_vcpu_by_mpidr(vm_t* vm, uint64_t mpidr)
{
    list_foreach(vm->vcpu_list, node_data_t, node)
    {
        vcpu_t *vcpu = (vcpu_t*) node->data;
        if ((vcpu->arch.sysregs.hyp.vmpidr_el2 & MPIDR_AFF_MSK) == (mpidr & MPIDR_AFF_MSK))  {
            return vcpu;
        }
    }

    return NULL;
}

static uint64_t vm_cpuid_to_mpidr(vm_t* vm, uint64_t cpuid)
{
    return platform_arch_cpuid_to_mpdir(&vm->config->platform, cpuid);
}

void vcpu_arch_init(vcpu_t* vcpu, vm_t* vm)
{
    vcpu->arch.sysregs.hyp.vmpidr_el2 = vm_cpuid_to_mpidr(vm, vcpu->id);
    vcpu->arch.sysregs.hyp.cntvoff_el2 = 0;

    vcpu->arch.psci_ctx.state = vcpu->id == 0 ? PSCI_ON : PSCI_OFF;

    uint64_t root_pt_pa;
    mem_translate(&cpu.as, vm->as.pt.root, &root_pt_pa);
    vcpu->arch.sysregs.hyp.vttbr_el2 =
        ((vcpu->vm->id << VTTBR_VMID_OFF) & VTTBR_VMID_MSK) |
        (root_pt_pa & ~VTTBR_VMID_MSK);

    ISB();  // make sure vmid is commited befor tlbi
    tlb_vm_inv_all(vm->id);

    vgic_cpu_init(vcpu);
}

void vcpu_arch_reset(vcpu_t* vcpu, uint64_t entry)
{
    memset(vcpu->regs, 0, sizeof(struct arch_regs));

    vcpu->arch.sysregs.hyp.elr_el2 = entry;
    vcpu->arch.sysregs.hyp.spsr_el2 =
        SPSR_EL1h | SPSR_F | SPSR_I | SPSR_A | SPSR_D;

    MSR(CNTVOFF_EL2, 0);

    /**
     *  See ARMv8-A ARM section D1.9.1 for registers that must be in a known
     * state at reset.
     */
    vcpu->arch.sysregs.vm.sctlr_el1 = SCTLR_RES1;
    MSR(CNTKCTL_EL1, 0);
    MSR(PMCR_EL0, 0);

    /**
     *  TODO: ARMv8-A ARM mentions another implementation optional registers
     * that reset to a known value.
     */
}

uint64_t vcpu_readreg(vcpu_t* vcpu, uint64_t reg)
{
    if (reg > 30) return 0;
    return vcpu->regs->x[reg];
}

void vcpu_writereg(vcpu_t* vcpu, uint64_t reg, uint64_t val)
{
    if (reg > 30) return;
    vcpu->regs->x[reg] = val;
}

uint64_t vcpu_readpc(vcpu_t* vcpu)
{
    uint64_t pc;
    if (vcpu->state == VCPU_ACTIVE) {
        pc = MRS(ELR_EL2);
    } else {
        pc = vcpu->arch.sysregs.hyp.elr_el2;
    }
    return pc;
}

void vcpu_writepc(vcpu_t* vcpu, uint64_t pc)
{
    if (vcpu->state == VCPU_ACTIVE) {
        MSR(ELR_EL2, pc);
    } else {
        vcpu->arch.sysregs.hyp.elr_el2 = pc;
    }
}

bool vm_readmem(vm_t* vm, void* dest, uintptr_t vmaddr, size_t n)
{
    // TODO

    return false;
}

void vcpu_arch_run(vcpu_t* vcpu)
{
    // TODO: consider using TPIDR_EL2 to store vcpu pointer
    if (vcpu->arch.psci_ctx.state == PSCI_ON) {
        vcpu_arch_entry();
    } else {
        cpu_idle();
    }
    
}

void vtimer_save_state(vcpu_t* vcpu) {

    uint64_t timer_ctl = MRS(CNTV_CTL_EL0);
    uint64_t timer_cmp = MRS(CNTV_CVAL_EL0);

    if((timer_ctl & 0x3) == 0x1 && vm_has_interrupt(vcpu->vm, 27) && 
        vgic_int_get_enabled(vcpu, 27)) {

        vcpu_t *next_vcpu = cpu.arch.vtimer.next_vcpu;
        if(next_vcpu != NULL){
            if((next_vcpu->arch.sysregs.vm.cntv_ctl_el0 & 0x3) == 0x1){
                if(next_vcpu->arch.sysregs.vm.cntv_cval_el0 > timer_cmp){
                    node_data_t *node = objcache_alloc(&partition->nodes);
                    node->data = next_vcpu;
                    list_push(&cpu.arch.vtimer.event_list, (node_t*)node);
                    cpu.arch.vtimer.next_vcpu = vcpu;
                    MSR(CNTHP_CTL_EL2, timer_ctl);
                    MSR(CNTHP_CVAL_EL2, timer_cmp);
                } 
            }
        } else {
            MSR(CNTHP_CTL_EL2, timer_ctl);
            MSR(CNTHP_CVAL_EL2, timer_cmp);
            cpu.arch.vtimer.next_vcpu = vcpu;
        }
    }
}

void vtimer_restore_state(vcpu_t *vcpu) {

    if(!vm_has_interrupt(vcpu->vm, 27)) {
        gic_set_enable(27, false);
        return;
    }

    vgic_hw_commit(vcpu, 27);

    if(cpu.arch.vtimer.next_vcpu == vcpu){
        node_data_t *node = (node_data_t*) list_pop(&cpu.arch.vtimer.event_list);
        if(node != NULL){
            vcpu_t *vcpu = node->data;
            objcache_free(&partition->nodes, node);
            cpu.arch.vtimer.next_vcpu = vcpu;
            MSR(CNTHP_CTL_EL2, vcpu->arch.sysregs.vm.cntv_ctl_el0);
            MSR(CNTHP_CVAL_EL2, vcpu->arch.sysregs.vm.cntv_cval_el0);
        } else {
            cpu.arch.vtimer.next_vcpu = NULL;
            MSR(CNTHP_CTL_EL2, 0x2);
        }
    }
}

void vcpu_save_state(vcpu_t* vcpu){
    if(vcpu == NULL) return;
    vcpu->arch.sysregs.hyp.elr_el2      = MRS(ELR_EL2);
    vcpu->arch.sysregs.hyp.spsr_el2     = MRS(SPSR_EL2);
    vcpu->arch.sysregs.vm.vbar_el1      = MRS(VBAR_EL1);
    vcpu->arch.sysregs.vm.tpidr_el1     = MRS(TPIDR_EL1);
    vcpu->arch.sysregs.vm.mair_el1      = MRS(MAIR_EL1);
    vcpu->arch.sysregs.vm.amair_el1     = MRS(AMAIR_EL1);
    vcpu->arch.sysregs.vm.tcr_el1       = MRS(TCR_EL1);
    vcpu->arch.sysregs.vm.ttbr0_el1     = MRS(TTBR0_EL1);
    vcpu->arch.sysregs.vm.ttbr1_el1     = MRS(TTBR1_EL1);
    vcpu->arch.sysregs.vm.sp_el0        = MRS(SP_EL0);
    vcpu->arch.sysregs.vm.sp_el1        = MRS(SP_EL1);
    vcpu->arch.sysregs.vm.spsr_el1      = MRS(SPSR_EL1);
    vcpu->arch.sysregs.vm.sctlr_el1     = MRS(SCTLR_EL1);
    vcpu->arch.sysregs.vm.actlr_el1     = MRS(ACTLR_EL1);
    vcpu->arch.sysregs.vm.par_el1       = MRS(PAR_EL1);
    vcpu->arch.sysregs.vm.far_el1       = MRS(FAR_EL1);
    vcpu->arch.sysregs.vm.esr_el1       = MRS(ESR_EL1);
    vcpu->arch.sysregs.vm.elr_el1       = MRS(ELR_EL1);
    vcpu->arch.sysregs.vm.afsr0_el1     = MRS(AFSR0_EL1);
    vcpu->arch.sysregs.vm.afsr1_el1     = MRS(AFSR1_EL1);
    vcpu->arch.sysregs.vm.tpidrro_el0   = MRS(TPIDRRO_EL0);
    vcpu->arch.sysregs.vm.tpidr_el0     = MRS(TPIDR_EL0);
    vcpu->arch.sysregs.vm.cntv_ctl_el0  = MRS(CNTV_CTL_EL0);
    vcpu->arch.sysregs.vm.cntv_cval_el0 = MRS(CNTV_CVAL_EL0);
    vcpu->arch.sysregs.vm.cntkctl_el1   = MRS(CNTKCTL_EL1);
    vgic_save_state(vcpu);
    vtimer_save_state(vcpu);
}

void vcpu_restore_state(vcpu_t* vcpu){
    if(vcpu == NULL) return;
    MSR(ELR_EL2, vcpu->arch.sysregs.hyp.elr_el2);
    MSR(SPSR_EL2, vcpu->arch.sysregs.hyp.spsr_el2);
    MSR(VTTBR_EL2, vcpu->arch.sysregs.hyp.vttbr_el2);
    MSR(VMPIDR_EL2, vcpu->arch.sysregs.hyp.vmpidr_el2);
    MSR(CNTVOFF_EL2, vcpu->arch.sysregs.hyp.cntvoff_el2);
    MSR(VBAR_EL1, vcpu->arch.sysregs.vm.vbar_el1);
    MSR(TPIDR_EL1, vcpu->arch.sysregs.vm.tpidr_el1);
    MSR(MAIR_EL1, vcpu->arch.sysregs.vm.mair_el1);
    MSR(AMAIR_EL1, vcpu->arch.sysregs.vm.amair_el1);
    MSR(TCR_EL1, vcpu->arch.sysregs.vm.tcr_el1);
    MSR(TTBR0_EL1, vcpu->arch.sysregs.vm.ttbr0_el1);
    MSR(TTBR1_EL1, vcpu->arch.sysregs.vm.ttbr1_el1);
    MSR(SP_EL0, vcpu->arch.sysregs.vm.sp_el0);
    MSR(SP_EL1, vcpu->arch.sysregs.vm.sp_el1);
    MSR(SPSR_EL1, vcpu->arch.sysregs.vm.spsr_el1);
    MSR(SCTLR_EL1, vcpu->arch.sysregs.vm.sctlr_el1);
    MSR(ACTLR_EL1, vcpu->arch.sysregs.vm.actlr_el1);
    MSR(PAR_EL1, vcpu->arch.sysregs.vm.par_el1);
    MSR(FAR_EL1, vcpu->arch.sysregs.vm.far_el1);
    MSR(ESR_EL1, vcpu->arch.sysregs.vm.esr_el1);
    MSR(ELR_EL1, vcpu->arch.sysregs.vm.elr_el1);
    MSR(AFSR0_EL1, vcpu->arch.sysregs.vm.afsr0_el1);
    MSR(AFSR1_EL1, vcpu->arch.sysregs.vm.afsr1_el1);
    MSR(TPIDRRO_EL0, vcpu->arch.sysregs.vm.tpidrro_el0);
    MSR(TPIDR_EL0, vcpu->arch.sysregs.vm.tpidr_el0);
    MSR(CNTV_CTL_EL0, vcpu->arch.sysregs.vm.cntv_ctl_el0);
    MSR(CNTV_CVAL_EL0, vcpu->arch.sysregs.vm.cntv_cval_el0);
    MSR(CNTKCTL_EL1, vcpu->arch.sysregs.vm.cntkctl_el1);
    vgic_restore_state(vcpu);
    vtimer_restore_state(vcpu);
}
