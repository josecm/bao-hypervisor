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

#include <arch/gic.h>
#include <arch/gicv2.h>

#include <bit.h>
#include <spinlock.h>
#include <cpu.h>
#include <interrupts.h>
#include <vm.h>

extern volatile struct gicd_hw gicd;
extern spinlock_t gicd_lock;

volatile struct gicc_hw gicc __attribute__((section(".devices"), aligned(PAGE_SIZE)));
volatile struct gich_hw gich __attribute__((section(".devices"), aligned(PAGE_SIZE)));

static cpuid_t gic_cpu_map[GIC_MAX_TARGETS];

size_t NUM_LRS;
/**
 * @brief Discovers the number of list registers available in the current 
 * GIC implementation.
 * 
 * @return number of available list registers
 */
size_t gich_num_lrs()
{
    return ((gich.VTR & GICH_VTR_MSK) >> GICH_VTR_OFF) + 1;
}

/**
 * @brief Initialize the GIC cpu interface.
 * 
 * It setups up configuration registers with predefined values, and updates
 * the global gic_cpu_map with the gic target id for the calling cpu.
 * 
 * @throw the cpu panics if it can't find out the id of the calling cpus gic 
 * interface (this id is the bit position in the SGIR CPUTargetList field
 * corresponding to this cpu).
 * 
 */
static inline void gicc_init()
{
    for (size_t i = 0; i < gich_num_lrs(); i++) {
        gich.LR[i] = 0;
    }

    gicc.PMR = GIC_LOWEST_PRIO;
    gicc.CTLR |= GICC_CTLR_EN_BIT | GICC_CTLR_EOImodeNS_BIT;

    gich.HCR |= GICH_HCR_LRENPIE_BIT;
    
    uint32_t sgi_targets = gicd.ITARGETSR[0] & BIT32_MASK(0, GIC_TARGET_BITS);
    ssize_t gic_cpu_id = 
        bitmap_find_nth((bitmap_t*)&sgi_targets, GIC_TARGET_BITS, 1, 0, true);
    if(gic_cpu_id < 0) {
        ERROR("cant find gic cpu id");
    }

    gic_cpu_map[cpu.id] = (cpuid_t)gic_cpu_id;
}

/**
 * @brief Save the CPU GIC state
 * 
 * @param state a pointer to the structure where the state will be saved
 * 
 * @note this function is meant to be used when entering a low power state via
 * PSCI
 */
void gicc_save_state(struct gicc_state *state)
{
    state->CTLR = gicc.CTLR;
    state->PMR = gicc.PMR;
    state->BPR = gicc.BPR;
    state->IAR = gicc.IAR;
    state->EOIR = gicc.EOIR;
    state->RPR = gicc.RPR;
    state->HPPIR = gicc.HPPIR;
    state->priv_ISENABLER = gicd.ISENABLER[0];

    for (size_t i = 0; i < GIC_NUM_PRIO_REGS(GIC_CPU_PRIV); i++) {
        state->priv_IPRIORITYR[i] = gicd.IPRIORITYR[i];
    }

    state->HCR = gich.HCR;
    for (size_t i = 0; i < gich_num_lrs(); i++) {
        state->LR[i] = gich.LR[i];
    }
}

/**
 * @brief Restore the CPU GIC state
 * 
 * @param state a pointer to the structure where the state was saved
 * 
 * @note this function is meant to be used when waking-up a low power state via
 * PSCI
 */
void gicc_restore_state(struct gicc_state *state)
{
    gicc.CTLR = state->CTLR;
    gicc.PMR = state->PMR;
    gicc.BPR = state->BPR;
    gicc.IAR = state->IAR;
    gicc.EOIR = state->EOIR;
    gicc.RPR = state->RPR;
    gicc.HPPIR = state->HPPIR;
    gicd.ISENABLER[0] = state->priv_ISENABLER;

    for (size_t i = 0; i < GIC_NUM_PRIO_REGS(GIC_CPU_PRIV); i++) {
        gicd.IPRIORITYR[i] = state->priv_IPRIORITYR[i];
    }

    gich.HCR = state->HCR;
    for (size_t i = 0; i < gich_num_lrs(); i++) {
        gich.LR[i] = state->LR[i];
    }
}

/**
 * @brief Initialize the CPU GIC private state
 * 
 * This includes the Distributor pre-cpu aliased register as well as the
 * cpu interface.
 * 
 */

void gic_cpu_init()
{
    for (size_t i = 0; i < GIC_NUM_INT_REGS(GIC_CPU_PRIV); i++) {
        /**
         * Make sure all private interrupts are not enabled, non pending,
         * non active.
         */
        gicd.ICENABLER[i] = -1;
        gicd.ICPENDR[i] = -1;
        gicd.ICACTIVER[i] = -1;
    }

    /* Clear any pending SGIs. */
    for (size_t i = 0; i < GIC_NUM_SGI_REGS; i++) {
        gicd.CPENDSGIR[i] = -1;
    }

    for (size_t i = 0; i < GIC_NUM_PRIO_REGS(GIC_CPU_PRIV); i++) {
        gicd.IPRIORITYR[i] = -1;
    }

    gicc_init();
}

/**
 * @brief Map the GICv2 MMIO interfaces: the cpu interface (gicc), 
 * the hypervisor virtual interface control (gich) and the distributor (gicd).
 * 
 * @note updates the hypervisor page tables, therefore, there might be
 * dynamic allocation of pages used for the page-tables during the mapping
 * process.
 * 
 */
void gic_map_mmio()
{
    mem_map_dev(&cpu.as, (vaddr_t)&gicc, platform.arch.gic.gicc_addr,
                NUM_PAGES(sizeof(gicc)));
    mem_map_dev(&cpu.as, (vaddr_t)&gich, platform.arch.gic.gich_addr,
                NUM_PAGES(sizeof(gich)));
    mem_map_dev(&cpu.as, (vaddr_t)&gicd, platform.arch.gic.gicd_addr,
                NUM_PAGES(sizeof(gicd)));
}

/**
 * @brief Send an sgi to the target cpu
 * 
 * @param cpu_target the id of the target cpu
 * @param sgi_num the sgi id
 * 
 * @note it performs the operation only if both parameters are valid, otherwise
 * it fails silently.
 */
void gic_send_sgi(cpuid_t cpu_target, irqid_t sgi_num)
{
    if (sgi_num < GIC_MAX_SGIS && cpu_target < GIC_MAX_TARGETS) {
        gicd.SGIR = 
            (1UL << (GICD_SGIR_CPUTRGLST_OFF + gic_cpu_map[cpu_target])) |
            (sgi_num & GICD_SGIR_SGIINTID_MSK);
    }
}

/**
 * @brief Translate a bitmap representing the internal cpu ids, to the bitmap
 * of ids used by the gic in the SGIR.CPUTargetList. These might differ on 
 * platforms with multiple clusters.
 * 
 * @param cpu_targets the cpu internal id bitmap
 * @return uint8_t the gic cpu id bitmap
 */
static inline uint8_t gic_translate_cpu_to_trgt(uint8_t cpu_targets) {
    uint8_t gic_targets = 0;
    for(size_t i = 0; i < GIC_MAX_TARGETS; i++) {
        if((1 << i) & cpu_targets) {
            gic_targets |= (1 << gic_cpu_map[i]);
        }
    }
    return gic_targets;
}

/**
 * @brief Set the interrupt targets in the GIC distributor.
 * 
 * @param int_id the interrupt id
 * @param cpu_targets a bitmap representing the target cpus (using hypervisor
 * internal cpu ids)
 */
void gicd_set_trgt(irqid_t int_id, uint8_t cpu_targets)
{
    size_t reg_ind = GIC_TARGET_REG(int_id);
    size_t off = GIC_TARGET_OFF(int_id);
    uint32_t mask = BIT32_MASK(off, GIC_TARGET_BITS);

    spin_lock(&gicd_lock);

    gicd.ITARGETSR[reg_ind] = (gicd.ITARGETSR[reg_ind] & ~mask) | 
        ((gic_translate_cpu_to_trgt(cpu_targets) << off) & mask);

    spin_unlock(&gicd_lock);
}

/**
 * @brief Set the interrupt priority in the GIC distributor
 * 
 * @param int_id the interrupt id
 * @param prio the priority value
 */
void gic_set_prio(irqid_t int_id, uint8_t prio)
{
    gicd_set_prio(int_id, prio);
}

/**
 * @brief Read the interrupt priority from the gic distributor
 * 
 * @param int_id the interrupt id
 * @return the priority value
 */
uint8_t gic_get_prio(irqid_t int_id)
{
    return gicd_get_prio(int_id);
}

/**
 * @brief Set the level/trigger interrupt sensitivity configuration in the GIC
 * 
 * @param int_id the interrupt id
 * @param cfg the interrupt configuration value (encoded in a 2-bit value)
 */
void gic_set_icfgr(irqid_t int_id, uint8_t cfg)
{
    gicd_set_icfgr(int_id, cfg);
}

/**
 * @brief Read the pending status of an interrupt from the GIC
 * 
 * @param int_id the interrupt id
 * @param pend the interrupt status (true if pending, false otherwise)
 */
bool gic_get_pend(irqid_t int_id)
{
    return gicd_get_pend(int_id);
}

/**
 * @brief Set the active status of an interrupt in the GIC
 * 
 * @param int_id the interrupt id
 * @param act the intended status (true if active, false otherwise)
 */
void gic_set_act(irqid_t int_id, bool act)
{
    gicd_set_act(int_id, act);
}

/**
 * @brief Read the active status of an interrupt from the GIC
 * 
 * @param int_id the interrupt id
 * @param act the active status (true if active, false otherwise)
 */
bool gic_get_act(irqid_t int_id)
{
    return gicd_get_act(int_id);
}

/**
 * @brief Enable/Disable an interrupt in the GIC
 * 
 * @param int_id the interrupt id
 * @param en the intended sate (true if enable, false if disabled)
 */
void gic_set_enable(irqid_t int_id, bool en)
{
    gicd_set_enable(int_id, en);
}

/**
 * @brief Set the pending status of an interrupt in the GIC
 * 
 * @param int_id the interrupt id
 * @param pend the intended status (true if pending, false otherwise)
 * 
 * @note it needs to differentiate between sgis and other interrupts as these
 * use special registers for modifying the pending set
 * 
 * @note gicv2 differentiates the sgi pending state for each possible cpu 
 * source. This function sets the pending state for the calling cpu (itself)
 * as source.
 */
void gic_set_pend(irqid_t int_id, bool pend)
{
    if (gic_is_sgi(int_id)) {
        size_t reg_ind = GICD_SGI_REG(int_id);
        size_t off = GICD_SGI_OFF(int_id);

        if (pend) {
            gicd.SPENDSGIR[reg_ind] = (1U) << (off + cpu.id);
        } else {
            gicd.CPENDSGIR[reg_ind] = BIT32_MASK(off, 8);
        }
    } else {
        gicd_set_pend(int_id, pend);
    }
}
