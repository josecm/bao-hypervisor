/**
 * Bao, a Lightweight Static Partitioning Hypervisor
 *
 * Copyright (c) Bao Project (www.bao-project.org), 2019-
 *
 * Authors:
 *      Jose Martins <jose.martins@bao-project.org>
 *      Angelo Ruocco <angeloruocco90@gmail.com>
 *
 * Bao is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 2 as published by the Free
 * Software Foundation, with a special exception exempting guest code from such
 * license. See the COPYING file in the top-level directory for details.
 *
 */

#include <bao.h>
#include <interrupts.h>

#include <cpu.h>
#include <platform.h>
#include <arch/gic.h>
#include <mem.h>
#include <arch/sysregs.h>
#include <vm.h>

#ifndef GIC_VERSION
#error "GIC_VERSION not defined for this platform"
#endif

/**
 * @brief Interrupt initialization for the arm architecture.
 * 
 * It initializes the GIC and sets up and enables the virtual GIC management
 * interrupt.
 */

void interrupts_arch_init()
{
    gic_init();

    if(cpu.id == CPU_MASTER) {
        interrupts_reserve(platform.arch.gic.maintenance_id,
            gic_maintenance_handler);
    }
    interrupts_cpu_enable(platform.arch.gic.maintenance_id, true);
}

/**
 * @brief Translate the higher interrupts_send_ipi function by to a 
 * gic_sgi_send call.
 * 
 * @param target_cpu the target cpu for the ipi
 * @param ipi_id the ipi id
 */

void interrupts_arch_ipi_send(cpuid_t target_cpu, irqid_t ipi_id)
{
    if (ipi_id < GIC_MAX_SGIS) gic_send_sgi(target_cpu, ipi_id);
}

/**
 * @brief Translate the higher level interrupts_cpu_enable function to the
 * base gic operations needed to enable an interrupt: enable it, set a priority
 * above the priority threshold and set the current cpu as a target fot the
 * interrupt.
 * 
 * 
 * @param int_id the interrupt id
 * @param en true to enable the interrupt, false to disable it
 * 
 * @note it setups up the priority and target parameter
 */

void interrupts_arch_enable(irqid_t int_id, bool en)
{
    gic_set_enable(int_id, en);
    gic_set_prio(int_id, 0x01);
    if (GIC_VERSION == GICV2) {
        gicd_set_trgt(int_id, 1 << cpu.id);
    } else {
        gicd_set_route(int_id, cpu.arch.mpidr);
    }
}

/**
 * @brief Translate the higher level interrupts_check to read the pending 
 * itnerrupt status through a gic driver call.
 * 
 * @param int_id the interrupt id
 * @return the pending status of the interrupt
 */
bool interrupts_arch_check(irqid_t int_id)
{
    return gic_get_pend(int_id);
}

/**
 * @brief Check if there a real conflict in assigning interrupts due to GIC 
 * architecture PPI alisasing. If the interrupt is a PPI it can be assigned
 * to multiple VMs.
 * 
 * @param int_id the interrupt id
 * @return if there is a true conflict in the assignment of the interrupt
 */
inline bool interrupts_arch_conflict(bitmap_t* interrupt_bitmap, irqid_t int_id)
{
    return (bitmap_get(interrupt_bitmap, int_id) && int_id > GIC_CPU_PRIV);
}

/**
 * @brief Clear interrupt pending status
 * 
 * @param int_id the interrupt id
 */
void interrupts_arch_clear(irqid_t int_id)
{
    gic_set_act(int_id, false);
    gic_set_pend(int_id, false);
}

/**
 * @brief Assign an interrupt to a vm by marking it as an hardware/physical
 * interrupt in the virtual GIC
 * 
 * @param vm a pointer to the vm
 * @param id the interrupt id
 */
void interrupts_arch_vm_assign(struct vm *vm, irqid_t id)
{
    vgic_set_hw(vm, id);
}
