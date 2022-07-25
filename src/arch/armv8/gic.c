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

#include <arch/gic.h>

#if (GIC_VERSION == GICV2)
#include <arch/gicv2.h>
#elif (GIC_VERSION == GICV3)
#include <arch/gicv3.h>
#else 
#error "unknown GIV version " GIC_VERSION
#endif


#include <interrupts.h>
#include <cpu.h>
#include <spinlock.h>
#include <platform.h>

volatile struct gicd_hw gicd __attribute__((section(".devices"), aligned(PAGE_SIZE)));
spinlock_t gicd_lock;

/**
 * @brief Initialize the GIC distributor.
 * 
 * Its sets up configuration registers with predefined values and brings
 * interrupt state (pending/active) and configurations (enable/priority/target)
 * to a quiesced/default value.
 * 
 */

void gicd_init()
{
    size_t int_num = gic_num_irqs();

    /* Bring distributor to known state */
    for (size_t i = GIC_NUM_PRIVINT_REGS; i < GIC_NUM_INT_REGS(int_num); i++) {
        /**
         * Make sure all interrupts are not enabled, non pending,
         * non active.
         */
        gicd.ICENABLER[i] = -1;
        gicd.ICPENDR[i] = -1;
        gicd.ICACTIVER[i] = -1;
    }

    /* All interrupts have lowest priority possible by default */
    for (size_t i = GIC_NUM_PRIO_REGS(GIC_CPU_PRIV);
         i < GIC_NUM_PRIO_REGS(int_num); i++) {
        gicd.IPRIORITYR[i] = -1;
    }

    if (GIC_VERSION == GICV2) {
        /* No CPU targets for any interrupt by default */
        for (size_t i = GIC_NUM_TARGET_REGS(GIC_CPU_PRIV);
             i < GIC_NUM_TARGET_REGS(int_num); i++) {
            gicd.ITARGETSR[i] = 0;
        }

        /* Enable distributor */
        gicd.CTLR |= GICD_CTLR_EN_BIT;

    } else {
        for (size_t i = GIC_CPU_PRIV; i < GIC_MAX_INTERUPTS; i++) {
            gicd.IROUTER[i] = GICD_IROUTER_INV;
        }

        /* Enable distributor and affinity routing */
        gicd.CTLR |= GICD_CTLR_ARE_NS_BIT | GICD_CTLR_ENA_BIT;
    }

    /* ICFGR are platform dependent, lets leave them as is */

    /* No need to setup gicd.NSACR as all interrupts are  setup to group 1 */

    interrupts_reserve(platform.arch.gic.maintenance_id,
                       gic_maintenance_handler);
}

/**
 * @brief Mapped the needed  GIC MMIO interfaces.
 * 
 * @note It depends on the gic version (2 or 3/4) which interfaces are MMIO,
 * so this function is implemented at the gic version-specific files.
 * 
 */
void gic_map_mmio();

/**
 * @brief Initialize the GIC.
 * 
 * Only the master cpu maps the mmio structures, initializes the gic distributor
 * and initializes the global NUM_LRS. All other initializes cpu private gic 
 * structures which mainly consists of the cpu interface.
 * 
 */
void gic_init()
{
    if (cpu.id == CPU_MASTER) {
        gic_map_mmio();
        gicd_init();
        NUM_LRS = gich_num_lrs();
    }

    cpu_sync_barrier(&cpu_glb_sync);

    gic_cpu_init();
}

/**
 * @brief GIC low-level interrupt handler.
 * 
 * This function should be called directly from the assembly exception vector.
 * It is interacts with the GIC to acknowledge and end the interrupt calling
 * the arch-agnostic interrupt handler.
 * 
 */
void gic_handle()
{
    uint32_t ack = gicc_iar();
    irqid_t id = bit32_extract(ack, GICC_IAR_ID_OFF, GICC_IAR_ID_LEN);

    if (id < GIC_FIRST_SPECIAL_INTID) {
        enum irq_res res = interrupts_handle(id);
        gicc_eoir(ack);
        if (res == HANDLED_BY_HYP) gicc_dir(ack);
    }
}

/**
 * @brief Reads the interrupt priority from the GIC distributor
 * 
 * @param int_id the interrupt id
 * @return uint8_t the interrupt priority
 */
uint8_t gicd_get_prio(irqid_t int_id)
{
    size_t reg_ind = GIC_PRIO_REG(int_id);
    size_t off = GIC_PRIO_OFF(int_id);

    uint8_t prio =
        gicd.IPRIORITYR[reg_ind] >> off & BIT32_MASK(off, GIC_PRIO_BITS);

    return prio;
}

/**
 * @brief Writes the interrupt level/trigger sensitivity configuration to the
 * GIC distributor
 * 
 * @param int_id the interrupt id
 * @param cfg the interrupt configuration value (encoded in a 2-bit value)
 */
void gicd_set_icfgr(irqid_t int_id, uint8_t cfg)
{
    size_t reg_ind = (int_id * GIC_CONFIG_BITS) / (sizeof(uint32_t) * 8);
    size_t off = (int_id * GIC_CONFIG_BITS) % (sizeof(uint32_t) * 8);
    uint32_t mask = ((1U << GIC_CONFIG_BITS) - 1) << off;

    spin_lock(&gicd_lock);

    gicd.ICFGR[reg_ind] = (gicd.ICFGR[reg_ind] & ~mask) | ((cfg << off) & mask);

    spin_unlock(&gicd_lock);
}

/**
 * @brief Sets the interrupt priority in the gic distributor
 * 
 * @param int_id the interrupt id
 * @param prio the interupt priority (8-bit value)
 */
void gicd_set_prio(irqid_t int_id, uint8_t prio)
{
    size_t reg_ind = GIC_PRIO_REG(int_id);
    size_t off = GIC_PRIO_OFF(int_id);
    uint32_t mask = BIT32_MASK(off, GIC_PRIO_BITS);

    spin_lock(&gicd_lock);

    gicd.IPRIORITYR[reg_ind] =
        (gicd.IPRIORITYR[reg_ind] & ~mask) | ((prio << off) & mask);

    spin_unlock(&gicd_lock);
}

/**
 * @brief Set the pending status of an interrupt in the GIC distributor
 * 
 * @param int_id the interrupt id
 * @param pend the intended status (true if pending, false otherwise)
 */
void gicd_set_pend(irqid_t int_id, bool pend)
{
    size_t reg_ind = GIC_INT_REG(int_id);
    if (pend) {
        gicd.ISPENDR[reg_ind] = GIC_INT_MASK(int_id);
    } else {
        gicd.ICPENDR[reg_ind] = GIC_INT_MASK(int_id);
    }
}

/**
 * @brief Read the pending status of an interrupt from the GIC distributor
 * 
 * @param int_id the interrupt id
 * @param pend the interrupt status (true if pending, false otherwise)
 */
bool gicd_get_pend(irqid_t int_id)
{
    return (gicd.ISPENDR[GIC_INT_REG(int_id)] & GIC_INT_MASK(int_id)) != 0;
}

/**
 * @brief Set the active status of an interrupt in the GIC distributor
 * 
 * @param int_id the interrupt id
 * @param act the intended status (true if active, false otherwise)
 */
void gicd_set_act(irqid_t int_id, bool act)
{
    size_t reg_ind = GIC_INT_REG(int_id);

    if (act) {
        gicd.ISACTIVER[reg_ind] = GIC_INT_MASK(int_id);
    } else {
        gicd.ICACTIVER[reg_ind] = GIC_INT_MASK(int_id);
    }
}

/**
 * @brief Read the active status of an interrupt from the GIC distributor
 * 
 * @param int_id the interrupt id
 * @param act the active status (true if active, false otherwise)
 */
bool gicd_get_act(irqid_t int_id)
{
    return (gicd.ISACTIVER[GIC_INT_REG(int_id)] & GIC_INT_MASK(int_id)) != 0;
}

/**
 * @brief Enable/Disable an interrupt in the GIC distributor
 * 
 * @param int_id the interrupt id
 * @param en the intended sate (true if enable, false if disabled)
 */
void gicd_set_enable(irqid_t int_id, bool en)
{
    size_t reg_ind = GIC_INT_REG(int_id);
    uint32_t bit = GIC_INT_MASK(int_id);

    if (en) {
        gicd.ISENABLER[reg_ind] = bit;
    } else {
        gicd.ICENABLER[reg_ind] = bit;
    }
}
