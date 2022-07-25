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

#include <interrupts.h>

#include <cpu.h>
#include <vm.h>
#include <bitmap.h>
#include <string.h>

BITMAP_ALLOC(hyp_interrupt_bitmap, MAX_INTERRUPTS);
BITMAP_ALLOC(global_interrupt_bitmap, MAX_INTERRUPTS);

irq_handler_t interrupt_handlers[MAX_INTERRUPTS];

/**
 * @brief Send an ipi to the target cpu
 * 
 * @param target_cpu the id of the target cpu
 * @param ipi_id the ipi interrupt id
 */
inline void interrupts_cpu_sendipi(cpuid_t target_cpu, irqid_t ipi_id)
{
    interrupts_arch_ipi_send(target_cpu, ipi_id);
}

/**
 * @brief set enable for the target interrupt for the calling cpu
 * 
 * @param int_id the interrupt id
 * @param en true to enable the interrupt, false to disable it
 */
inline void interrupts_cpu_enable(irqid_t int_id, bool en)
{
    interrupts_arch_enable(int_id, en);
}


/**
 * @brief Check if the target interrupt is pending
 * 
 * @param int_id the interrupt id
 * @return interrupt pending status (true if pending, false if not)
 */
inline bool interrupts_check(irqid_t int_id)
{
    return interrupts_arch_check(int_id);
}

/**
 * @brief Clear the pending status of the target interrupt
 * 
 * @param int_id the interrupt id
 */
inline void interrupts_clear(irqid_t int_id)
{
    interrupts_arch_clear(int_id);
}

/**
 * @brief Initialize the interrupt subsystem. Calls the arch-specific
 * initializer and sets up and enables the hypervisor IPI.
 * 
 */
inline void interrupts_init()
{
    interrupts_arch_init();

    if (cpu.id == CPU_MASTER) {
        interrupts_reserve(IPI_CPU_MSG, cpu_msg_handler);
    }

    interrupts_cpu_enable(IPI_CPU_MSG, true);
}

/**
 * @brief Check if the target interrupt was reserved for hypervisor use
 * 
 * @param int_id the interrupt id
 * @return true if the interrupt is in use by the hypervisor
 */
static inline bool interrupt_is_reserved(irqid_t int_id)
{
    return bitmap_get(hyp_interrupt_bitmap, int_id);
}

/**
 * @brief The arch-agnostic interrupt handler. Checks if the received 
 * interrupt is targetted to the vm the current cpu is hosting or to the 
 * hypervisor. If neither is the case, the cpu panics.
 * 
 * @param int_id the received interrupt id
 * @return an enum irq_res value that informs if the received interrupt was 
 * handler by the hypervisor or forwarded to the vm
 * 
 * @throw error in case the interrupt is unknown
 */

enum irq_res interrupts_handle(irqid_t int_id)
{
    if (vm_has_interrupt(cpu.vcpu->vm, int_id)) {
        vcpu_inject_hw_irq(cpu.vcpu, int_id);

        return FORWARD_TO_VM;

    } else if (interrupt_is_reserved(int_id)) {
        interrupt_handlers[int_id](int_id);

        return HANDLED_BY_HYP;

    } else {
        ERROR("received unknown interrupt id = %d", int_id);
    }
}

/**
 * @brief Assign a physical interrupt to a given vm. 
 * 
 * The function panics if the interrupt was previously assigned.
 * 
 * @param vm a pointer to the vm being assigned the interrupt
 * @param id the interrupt id
 * 
 * @throw a conflict error if the interrupt was previsouly assigned
 * 
 * @note updates global_interrupt_bitmap global variable
 */
void interrupts_vm_assign(struct vm *vm, irqid_t id)
{
    if (interrupts_arch_conflict(global_interrupt_bitmap, id)) {
        ERROR("Interrupts conflict, id = %d\n", id);
    }

    interrupts_arch_vm_assign(vm, id);

    bitmap_set(vm->interrupt_bitmap, id);
    bitmap_set(global_interrupt_bitmap, id);
}

/**
 * @brief Reserve an interrupt for hypervisor and install an handler for it.
 * 
 * @param int_id the interrupt id
 * @param handler a function pointer to the interrupt handler
 *
 * @note updates global_interrupt_bitmap global and hyp_interrupt_bitmap 
 * variables
 */
void interrupts_reserve(irqid_t int_id, irq_handler_t handler)
{
    if (int_id < MAX_INTERRUPTS) {
        interrupt_handlers[int_id] = handler;
        bitmap_set(hyp_interrupt_bitmap, int_id);
        bitmap_set(global_interrupt_bitmap, int_id);
    }
}
