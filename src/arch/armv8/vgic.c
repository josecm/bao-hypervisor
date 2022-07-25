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

#include <arch/vgic.h>

#if (GIC_VERSION == GICV2)
#include <arch/gicv2.h>
#include <arch/vgicv2.h>
#elif (GIC_VERSION == GICV3)
#include <arch/gicv3.h>
#include <arch/vgicv3.h>
#else 
#error "unknown GIV version " GIC_VERSION
#endif

#include <bit.h>
#include <spinlock.h>
#include <cpu.h>
#include <interrupts.h>
#include <vm.h>

enum VGIC_EVENTS { VGIC_UPDATE_ENABLE, VGIC_ROUTE, VGIC_INJECT, VGIC_SET_REG };
extern volatile const uint64_t VGIC_IPI_ID;

#define GICD_IS_REG(REG, offset)            \
    (((offset) >= offsetof(struct gicd_hw, REG)) && \
     (offset) < (offsetof(struct gicd_hw, REG) + sizeof(gicd.REG)))
#define GICD_REG_GROUP(REG) ((offsetof(struct gicd_hw, REG) & 0xff80) >> 7)
#define GICD_REG_MASK(ADDR) ((ADDR)&(GIC_VERSION == GICV2 ? 0xfffULL : 0xffffULL))
#define GICD_REG_IND(REG) (offsetof(struct gicd_hw, REG) & 0x7f)

#define VGIC_MSG_DATA(VM_ID, VGICRID, INT_ID, REG, VAL)                 \
    (((uint64_t)(VM_ID) << 48) | (((uint64_t)(VGICRID)&0xffff) << 32) | \
     (((INT_ID)&0x7fff) << 16) | (((REG)&0xff) << 8) | ((VAL)&0xff))
#define VGIC_MSG_VM(DATA) ((DATA) >> 48)
#define VGIC_MSG_VGICRID(DATA) (((DATA) >> 32) & 0xffff)
#define VGIC_MSG_INTID(DATA) (((DATA) >> 16) & 0x7fff)
#define VGIC_MSG_REG(DATA) (((DATA) >> 8) & 0xff)
#define VGIC_MSG_VAL(DATA) ((DATA)&0xff)

void vgic_ipi_handler(uint32_t event, uint64_t data);
CPU_MSG_HANDLER(vgic_ipi_handler, VGIC_IPI_ID);

/**
 * @brief Get the interrupt structure from the virtual gic of the vm associated
 * with the target virtual cpu.
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param int_id the interrupt id
 * @param vgicr_id For gicv3/4 the id of the redistributor associated with the
 * interrupt in case of this being a PPI.
 * @return a pointer to the interrupt structure if there is a valid int_id
 * in the vgic, NULL otherwise
 */
static inline struct vgic_int *vgic_get_int(struct vcpu *vcpu, irqid_t int_id,
                                       vcpuid_t vgicr_id)
{
    if (int_id < GIC_CPU_PRIV) {
        struct vcpu *target_vcpu =
            vgicr_id == vcpu->id ? vcpu : vm_get_vcpu(vcpu->vm, vgicr_id);
        return &target_vcpu->arch.vgic_priv.interrupts[int_id];
    } else if (int_id < vcpu->vm->arch.vgicd.int_num) {
        return &vcpu->vm->arch.vgicd.interrupts[int_id - GIC_CPU_PRIV];
    }

    return NULL;
}

/**
 * @brief Checks if the interrupt is tied with the real physical interrupt
 * 
 * @param interrupt a pointer to the interrupt structure
 * @return true if the interrupt is associated with a physical one, false
 * otherwise
 * 
 * @note virtual sgis are never tied to a physical sgi
 */
static inline bool vgic_int_is_hw(struct vgic_int *interrupt)
{
    return !(interrupt->id < GIC_MAX_SGIS) && interrupt->hw;
}

/**
 * @brief Check if the interrupt is in a list register, returning the list
 * register id and its contents if so.
 * 
 * @param interrupt pointer to the interrupt structure
 * @param[out] lr the contents of the list register
 * @return int64_t the id of the list register, -1 if there was none
 */
static inline int64_t gich_get_lr(struct vgic_int *interrupt, unsigned long *lr)
{
    if (!interrupt->in_lr || interrupt->owner->phys_id != cpu.id) {
        return -1;
    }

    unsigned long lr_val = gich_read_lr(interrupt->lr);
    if ((GICH_LR_VID(lr_val) == interrupt->id) &&
        (GICH_LR_STATE(lr_val) != INV)) {
        if (lr != NULL) *lr = lr_val;
        return interrupt->lr;
    }

    return -1;
}

/**
 * @brief Get the interrupt state (state and/or pending)
 * 
 * @param interrupt pointer to the interrupt structure
 * @return uint8_t interrupt state enconding
 * 
 * @note if the interrupt is part of the a list register
 */
static inline uint8_t vgic_get_state(struct vgic_int *interrupt)
{
    uint8_t state = 0;

    unsigned long lr_val = 0;
    if (gich_get_lr(interrupt, &lr_val) >= 0) {
        state = GICH_LR_STATE(lr_val);
    } else {
        state = interrupt->state;
    }

#if (GIC_VERSION == GICV2)
    if (interrupt->id < GIC_MAX_SGIS && interrupt->sgi.pend) {
        state |= PEND;
    }
#endif

    return state;
}

/**
 * @brief Try to get the ownership of the interrupt for a given vcpu
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt structure
 * @return true if ownership was successfully attained, false otherwise
 */
bool vgic_get_ownership(struct vcpu *vcpu, struct vgic_int *interrupt)
{
    bool ret = false;

    if (interrupt->owner == vcpu) {
        ret = true;
    } else if (interrupt->owner == NULL) {
        interrupt->owner = vcpu;
        ret = true;
    }

    return ret;
}

/**
 * @brief Check if a vcpu currently owns a given interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt structure
 * @return true if vcpu currently owns the interrupt, false otherwise
 */
bool vgic_owns(struct vcpu *vcpu, struct vgic_int *interrupt)
{
    return interrupt->owner == vcpu;
}

/**
 * @brief try to yield the ownership of a given interrupt
 * 
 * The semantics of this function are just to *try* to yield the ownership of
 * a given interrupt. If this is not possible it will "fail" silently.
 * 
 * @note ownership cannot be yielded for private interrupts in gicv2, as cpus
 * cant access each others private interrupt state
 * 
 * @note if an interrupt is currently in an lr or its state is active, it
 * cannot be yielded
 * 
 */
void vgic_yield_ownership(struct vcpu *vcpu, struct vgic_int *interrupt)
{
    if ((GIC_VERSION == GICV2 && gic_is_priv(interrupt->id)) ||
        !vgic_owns(vcpu, interrupt) || interrupt->in_lr ||
        (vgic_get_state(interrupt) & ACT)) {
        return;
    }

    interrupt->owner = NULL;
}

/**
 * @brief Send a message to inject an sgi to other cpus
 * 
 * @param vcpu the source vcpu for the sgi
 * @param pcpu_mask a bitmap of the physical cpus to which to send the sgi
 * @param int_id the interrupt id of the sgi
 * 
 * @note as cpu messages are sent in this function, it implies the
 * allocation of a message node from a slab down the line.
 */
void vgic_send_sgi_msg(struct vcpu *vcpu, cpumap_t pcpu_mask, irqid_t int_id)
{
    struct cpu_msg msg = {
        VGIC_IPI_ID, VGIC_INJECT,
        VGIC_MSG_DATA(cpu.vcpu->vm->id, 0, int_id, 0, cpu.vcpu->id)};

    for (size_t i = 0; i < platform.cpu_num; i++) {
        if (pcpu_mask & (1ull << i)) {
            cpu_send_msg(i, &msg);
        }
    }
}

/**
 * @brief Route and inject an active and/or pending interrupt to the correct 
 * vcpu.
 *
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt structure
 * 
 * @note as cpu messages are sent in this function, it implies the
 * allocation of a message node from a slab down the line.
 */
void vgic_route(struct vcpu *vcpu, struct vgic_int *interrupt)
{
    if ((interrupt->state == INV) || !interrupt->enabled) {
        return;
    }

    if (vgic_int_vcpu_is_target(vcpu, interrupt)) {
        vgic_add_lr(vcpu, interrupt);
    }

    if (!interrupt->in_lr && vgic_int_has_other_target(vcpu, interrupt)) {
        struct cpu_msg msg = {
            VGIC_IPI_ID, VGIC_ROUTE,
            VGIC_MSG_DATA(vcpu->vm->id, vcpu->id, interrupt->id, 0, 0)};
        vgic_yield_ownership(vcpu, interrupt);
        cpumap_t trgtlist =
            vgic_int_ptarget_mask(vcpu, interrupt) & ~(1ull << vcpu->phys_id);
        for (size_t i = 0; i < platform.cpu_num; i++) {
            if (trgtlist & (1ull << i)) {
                cpu_send_msg(i, &msg);
            }
        }
    }
}

/**
 * @brief Write the interrupt to an LR
 * 
 * Also the function  if an interrupt previously written to the 
 * lr is still owned by the current vcpu and, if so, yields its ownership.
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt structure
 * @param lr_ind the index for the lr to be written
 * 
 */
static inline void vgic_write_lr(struct vcpu *vcpu, struct vgic_int *interrupt,
                                 size_t lr_ind)
{
    irqid_t prev_int_id = vcpu->arch.vgic_priv.curr_lrs[lr_ind];

    if ((prev_int_id != interrupt->id) && !gic_is_priv(prev_int_id)) {
        struct vgic_int *prev_interrupt = vgic_get_int(vcpu, prev_int_id, vcpu->id);
        if (prev_interrupt != NULL) {
            spin_lock(&prev_interrupt->lock);
            if (vgic_owns(vcpu, prev_interrupt) && prev_interrupt->in_lr &&
                (prev_interrupt->lr == lr_ind)) {
                prev_interrupt->in_lr = false;
                vgic_yield_ownership(vcpu, prev_interrupt);
            }
            spin_unlock(&prev_interrupt->lock);
        }
    }

    unsigned state = vgic_get_state(interrupt);

    unsigned long lr = ((interrupt->id << GICH_LR_VID_OFF) & GICH_LR_VID_MSK);

#if (GIC_VERSION == GICV2)
    lr |= (((unsigned long)interrupt->prio >> 3) << GICH_LR_PRIO_OFF) &
          GICH_LR_PRIO_MSK;
#else
    lr |= (((unsigned long)interrupt->prio << GICH_LR_PRIO_OFF) & GICH_LR_PRIO_MSK) |
          GICH_LR_GRP_BIT;
#endif

    if (vgic_int_is_hw(interrupt)) {
        lr |= GICH_LR_HW_BIT;
        lr |= ((unsigned long)interrupt->id << GICH_LR_PID_OFF) & GICH_LR_PID_MSK;
        if (state == PENDACT) {
            lr |= GICH_LR_STATE_ACT;
        } else {
            lr |= ((unsigned long)state << GICH_LR_STATE_OFF) & GICH_LR_STATE_MSK;
        }
    }
#if (GIC_VERSION == GICV2)
    else if (interrupt->id < GIC_MAX_SGIS) {
        if (state & ACT) {
            lr |= (interrupt->sgi.act << GICH_LR_CPUID_OFF) & GICH_LR_CPUID_MSK;
            lr |= GICH_LR_STATE_ACT;
        } else {
            for (ssize_t i = GIC_MAX_TARGETS - 1; i >= 0; i--) {
                if (interrupt->sgi.pend & (1U << i)) {
                    lr |= (i << GICH_LR_CPUID_OFF) & GICH_LR_CPUID_MSK;
                    interrupt->sgi.pend &= ~(1U << i);

                    lr |= GICH_LR_STATE_PND;
                    break;
                }
            }
        }

        if (interrupt->sgi.pend) {
            lr |= GICH_LR_EOI_BIT;
        }

    }
#endif
    else {
        if (!gic_is_priv(interrupt->id) && !vgic_int_is_hw(interrupt)) {
            lr |= GICH_LR_EOI_BIT;
        }

        lr |= ((unsigned long)state << GICH_LR_STATE_OFF) & GICH_LR_STATE_MSK;
    }

    interrupt->state = 0;
    interrupt->in_lr = true;
    interrupt->lr = lr_ind;
    vcpu->arch.vgic_priv.curr_lrs[lr_ind] = interrupt->id;
    gich_write_lr(lr_ind, lr);
}

/**
 * @brief Remove interrupt from LR.
 * 
 * Only works when the interrupt is owned by the calling cpu and is, in fact,
 * in a LR.
 * 
 * If the interrupt state is pending, it turns on the "no pending" maintenance
 * interrupt source so that the hypervisor is notified if no more pending
 * interrupts are present in the list registers.
 *
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt
 * @return true if interrupt was successfuly removed from lr, false otherwise
 */

bool vgic_remove_lr(struct vcpu *vcpu, struct vgic_int *interrupt)
{
    bool ret = false;

    if (!vgic_owns(vcpu, interrupt) || !interrupt->in_lr) {
        return ret;
    }

    unsigned long lr_val = 0;
    ssize_t lr_ind = -1;
    if ((lr_ind = gich_get_lr(interrupt, &lr_val)) >= 0) {
        gich_write_lr(lr_ind, 0);
    }

    interrupt->in_lr = false;

    if (GICH_LR_STATE(lr_val) != INV) {
        interrupt->state = GICH_LR_STATE(lr_val);
#if (GIC_VERSION == GICV2)
        if (interrupt->id < GIC_MAX_SGIS) {
            if (interrupt->state & ACT) {
                interrupt->sgi.act = GICH_LR_CPUID(lr_val);
            } else if (interrupt->state & PEND) {
                interrupt->sgi.pend |= (1U << GICH_LR_CPUID(lr_val));
            }
        }
#endif
        uint32_t hcr = gich_get_hcr();
        if ((interrupt->state & PEND) && interrupt->enabled) {
            hcr |= GICH_HCR_NPIE_BIT;
        }
        gich_set_hcr(hcr | GICH_HCR_UIE_BIT);

        ret = true;
    }

    return ret;
}

/**
 * @brief Add interrupt to the vgic list of spilled interrupts
 * 
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt
 * 
 * @note a spilled interrupt is an interrupt that would ideally be placed in
 * a LR (because is pending and/or active) but cannot be because there are
 * not enough LRs
 */

void vgic_add_spilled(struct vcpu *vcpu, struct vgic_int* interrupt) {
    spin_lock(&vcpu->vm->arch.vgic_spilled_lock);
    struct list *spilled_list = NULL;
    if (gic_is_priv(interrupt->id)) {
        spilled_list = &vcpu->arch.vgic_spilled;
    } else {
        spilled_list = &vcpu->vm->arch.vgic_spilled;
    }
    list_push(spilled_list, (node_t*)interrupt);
    spin_unlock(&vcpu->vm->arch.vgic_spilled_lock);
}

/**
 * @brief Spill the interrupt present in a LR
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param lr_ind the index of the LR containing the interrupt to be spilled
 */
void vgic_spill_lr(struct vcpu *vcpu, unsigned lr_ind) {
    unsigned long lr = gich_read_lr(lr_ind);
    struct vgic_int *spilled_int = vgic_get_int(vcpu, GICH_LR_VID(lr), vcpu->id);

    if (spilled_int != NULL) {
        spin_lock(&spilled_int->lock);
        vgic_remove_lr(vcpu, spilled_int);
        vgic_add_spilled(vcpu, spilled_int);
        vgic_yield_ownership(vcpu, spilled_int);
        spin_unlock(&spilled_int->lock);
    }
}


/**
 * @brief Add an interrupt to an LR, allocating an LR in the process.
 * 
 * If there is no available/free LR, the function tries to maintain the following
 * rules (as suggested by the GIC specification):
 * 
 *  - if any vgic interrupt is pending, at least one pending must be present 
 * in the list registers;
 *  - maximize the number of active interrupts in the list register;
 *  - always select the higher priority interrupts available;
 * 
 * In case there is not a free LR, one interrupt will be spilled and added to 
 * a spilled list (depending on wether it is a PPI os SPI).
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @return true if the interrupt was successfully added to an lr, false 
 * otherwise
 */
bool vgic_add_lr(struct vcpu *vcpu, struct vgic_int *interrupt)
{
    bool ret = false;

    if (!interrupt->enabled || interrupt->in_lr) {
        return ret;
    }

    ssize_t lr_ind = -1;
    uint64_t elrsr = gich_get_elrsr();
    for (size_t i = 0; i < NUM_LRS; i++) {
        if (bit64_get(elrsr, i)) {
            lr_ind = i;
            break;
        }
    }

    if (lr_ind < 0) {
        unsigned min_prio_pend = interrupt->prio, min_prio_act = interrupt->prio;
        unsigned min_id_act = interrupt->id, min_id_pend = interrupt->id;
        size_t pend_found = 0, act_found = 0;
        ssize_t pend_ind = -1, act_ind = -1;

        for (size_t i = 0; i < NUM_LRS; i++) {
            unsigned long lr = gich_read_lr(i);
            unsigned lr_id = GICH_LR_VID(lr);
            unsigned lr_prio = (lr & GICH_LR_PRIO_MSK) >> GICH_LR_PRIO_OFF;
            if (GIC_VERSION == GICV2) {
                lr_prio = lr_prio << 3;
            }
            unsigned lr_state = (lr & GICH_LR_STATE_MSK);

            if (lr_state & GICH_LR_STATE_ACT) {
                if (lr_prio > min_prio_act ||
                    (lr_prio == min_prio_act && lr_id > min_id_act)) {
                    min_id_act = lr_id;
                    min_prio_act = lr_prio;
                    act_ind = i;
                }
                act_found++;
            } else if (lr_state & GICH_LR_STATE_PND) {
                if (lr_prio > min_prio_pend ||
                    (lr_prio == min_prio_pend && lr_id > min_id_pend)) {
                    min_id_pend = lr_id;
                    min_prio_pend = lr_prio;
                    pend_ind = i;
                }
                pend_found++;
            }
        }

        if (pend_found > 1) {
            lr_ind = pend_ind;
        } else {
            lr_ind = act_ind;
        }

        if (lr_ind >= 0) {
            vgic_spill_lr(vcpu, lr_ind);
        }
    }

    if (lr_ind >= 0) {
        vgic_write_lr(vcpu, interrupt, lr_ind);
        ret = true;
    } else {
        vgic_add_spilled(vcpu, interrupt);
    }

    return ret;
}

#define VGIC_ENABLE_MASK \
    ((GIC_VERSION == GICV2) ? GICD_CTLR_EN_BIT : GICD_CTLR_ENA_BIT)

/**
 * @brief Update the enable stat of the virtual gic.
 * 
 * This is done by enabling the virtual cpu interface.
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 */
static inline void vgic_update_enable(struct vcpu *vcpu)
{
    if (cpu.vcpu->vm->arch.vgicd.CTLR & VGIC_ENABLE_MASK) {
        gich_set_hcr(gich_get_hcr() | GICH_HCR_En_BIT);
    } else {
        gich_set_hcr(gich_get_hcr() & ~GICH_HCR_En_BIT);
    }
}

/**
 * @brief Emulation handler for a number of misc functionality virtual gic 
 * distributor registers: GICD.CTLR, GICD.TYPER, GICD.IIDR.
 * 
 * @param acc pointer to a emulation struct containing information about the
 * guest access
 * @param handlers pointer to a vgic register access handling information structure
 * @param gicr_access boolean indicating if this is an access to a 
 * redistributor (must be false)
 * @param vgicr_id the gicr id in case this is a gicr access (ignored)
 */
void vgicd_emul_misc_access(struct emul_access *acc,
                            struct vgic_reg_handler_info *handlers,
                            bool gicr_access, cpuid_t vgicr_id)
{
    struct vgicd *vgicd = &cpu.vcpu->vm->arch.vgicd;
    unsigned reg = acc->addr & 0x7F;

    switch (reg) {
        case GICD_REG_IND(CTLR):
            if (acc->write) {
                uint32_t prev_ctrl = vgicd->CTLR;
                vgicd->CTLR =
                    vcpu_readreg(cpu.vcpu, acc->reg) & VGIC_ENABLE_MASK;
                if (prev_ctrl ^ vgicd->CTLR) {
                    vgic_update_enable(cpu.vcpu);
                    struct cpu_msg msg = {
                        VGIC_IPI_ID, VGIC_UPDATE_ENABLE,
                        VGIC_MSG_DATA(cpu.vcpu->vm->id, 0, 0, 0, 0)};
                    vm_msg_broadcast(cpu.vcpu->vm, &msg);
                }
            } else {
                vcpu_writereg(cpu.vcpu, acc->reg,
                              vgicd->CTLR | GICD_CTLR_ARE_NS_BIT);
            }
            break;
        case GICD_REG_IND(TYPER):
            if (!acc->write) {
                vcpu_writereg(cpu.vcpu, acc->reg, vgicd->TYPER);
            }
            break;
        case GICD_REG_IND(IIDR):
            if (!acc->write) {
                vcpu_writereg(cpu.vcpu, acc->reg, vgicd->IIDR);
            }
            break;
    }
}

/**
 * @brief Emulation handler for the GICD.PIDR register access in the virtual gic.
 * 
 * @param acc pointer to a emulation struct containing information about the
 * guest access
 * @param handlers pointer to a vgic register access handling information structure
 * @param gicr_access boolean indicating if this is an access to a 
 * redistributor (must be false)
 * @param vgicr_id the gicr id in case this is a gicr access (ignored)
 */
void vgicd_emul_pidr_access(struct emul_access *acc,
                            struct vgic_reg_handler_info *handlers,
                            bool gicr_access, cpuid_t vgicr_id)
{
    if (!acc->write) {
        vcpu_writereg(cpu.vcpu, acc->reg,
                      gicd.ID[((acc->addr & 0xff) - 0xd0) / 4]);
    }
}

/**
 * @brief Update the enable state of a given vgic interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @param enable new enable state
 * @return true if the update modified the interrupt state, false otherwise
 */
bool vgic_int_update_enable(struct vcpu *vcpu, struct vgic_int *interrupt, bool enable)
{
    if (GIC_VERSION == GICV2 && gic_is_sgi(interrupt->id)) {
        return false;
    }

    if (enable != interrupt->enabled) {
        interrupt->enabled = enable;
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Commit the interrupt enable state to the real physical interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 */
void vgic_int_enable_hw(struct vcpu *vcpu, struct vgic_int *interrupt)
{
#if (GIC_VERSION != GICV2)
    if (gic_is_priv(interrupt->id)) {
        gicr_set_enable(interrupt->id, interrupt->enabled,
                        interrupt->phys.redist);
    } else {
        gicd_set_enable(interrupt->id, interrupt->enabled);
    }
#else
    gic_set_enable(interrupt->id, interrupt->enabled);
#endif
}

/**
 * @brief Clear the enable state of the interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @param data interpreted as a boolean indicating to clear the enable state or
 * not
 * @return true if the state was updated, false otherwise
 */
bool vgic_int_clear_enable(struct vcpu *vcpu, struct vgic_int *interrupt, uint64_t data)
{
    if (!data)
        return false;
    else
        return vgic_int_update_enable(vcpu, interrupt, false);
}

/**
 * @brief Set the enable state of the interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @param data interpreted as a boolean indicating to set the enable state or
 * not
 * @return true if the enable state was updated, false otherwise
 */
bool vgic_int_set_enable(struct vcpu *vcpu, struct vgic_int *interrupt, uint64_t data)
{
    if (!data)
        return false;
    else
        return vgic_int_update_enable(vcpu, interrupt, true);
}

/**
 * @brief Read the enable state of the interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @return uint64_t casted from bool: 1 if the interrupt is enabled, 0 if not
 */
uint64_t vgic_int_get_enable(struct vcpu *vcpu, struct vgic_int *interrupt)
{
    return (uint64_t)interrupt->enabled;
}

/**
 * @brief Update the pending state of the interrupt.
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @param pend indicates the pretended state for the interrupt pending
 * @return true if the pending state was updated, false otherwise 
 */
bool vgic_int_update_pend(struct vcpu *vcpu, struct vgic_int *interrupt, bool pend)
{
    if (GIC_VERSION == GICV2 && gic_is_sgi(interrupt->id)) {
        return false;
    }

    if (pend ^ !!(interrupt->state & PEND)) {
        if (pend)
            interrupt->state |= PEND;
        else
            interrupt->state &= ~PEND;
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Commit the interrupt active/pending state to the real physical interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 */
void vgic_int_state_hw(struct vcpu *vcpu, struct vgic_int *interrupt)
{
    uint8_t state = interrupt->state == PEND ? ACT : interrupt->state;
    bool pend = (state & PEND) != 0;
    bool act = (state & ACT) != 0;
#if (GIC_VERSION != GICV2)
    if (gic_is_priv(interrupt->id)) {
        gicr_set_act(interrupt->id, act, interrupt->phys.redist);
        gicr_set_pend(interrupt->id, pend, interrupt->phys.redist);
    } else {
        gicd_set_act(interrupt->id, act);
        gicd_set_pend(interrupt->id, pend);
    }
#else
    gicd_set_act(interrupt->id, act);
    gicd_set_pend(interrupt->id, pend);
#endif
}

/**
 * @brief Clear the pending state of the interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @param data interpreted as a boolean indicating to clear the pending state or
 * not
 * @return true if the state was updated, false otherwise
 */
bool vgic_int_clear_pend(struct vcpu *vcpu, struct vgic_int *interrupt, uint64_t data)
{
    if (!data)
        return false;
    else
        return vgic_int_update_pend(vcpu, interrupt, false);
}

/**
 * @brief Set the pending state of the interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @param data interpreted as a boolean indicating to set the pending state or
 * not
 * @return true if the enable state was updated, false otherwise
 */
bool vgic_int_set_pend(struct vcpu *vcpu, struct vgic_int *interrupt, uint64_t data)
{
    if (!data)
        return false;
    else
        return vgic_int_update_pend(vcpu, interrupt, true);
}

/**
 * @brief Read the pending state of the interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @return uint64_t 1 if the interrupt is pending, 0 otherwise
 */
uint64_t vgic_int_get_pend(struct vcpu *vcpu, struct vgic_int *interrupt)
{
    return (interrupt->state & PEND) ? 1 : 0;
}

/**
 * @brief Update the active state of a given vgic interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @param act new act state
 * @return true if the update modified the interrupt state, false otherwise
 */
bool vgic_int_update_act(struct vcpu *vcpu, struct vgic_int *interrupt, bool act)
{
    if (act ^ !!(interrupt->state & ACT)) {
        if (act)
            interrupt->state |= ACT;
        else
            interrupt->state &= ~ACT;
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Clear the active state of the vgic interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @param data interpreted as a boolean indicating to clear the active state or
 * not
 * @return true if the state was updated, false otherwise
 */
bool vgic_int_clear_act(struct vcpu *vcpu, struct vgic_int *interrupt, uint64_t data)
{
    if (!data)
        return false;
    else
        return vgic_int_update_act(vcpu, interrupt, false);
}

/**
 * @brief Set the active state of the vgic interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @param data interpreted as a boolean indicating to set the active state or
 * not
 * @return true if the enable state was updated, false otherwise
 */
bool vgic_int_set_act(struct vcpu *vcpu, struct vgic_int *interrupt, uint64_t data)
{
    if (!data)
        return false;
    else
        return vgic_int_update_act(vcpu, interrupt, true);
}

/**
 * @brief Read the active state of the vgic interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @return uint64_t 1 if the interrupt is active, 0 otherwise 
 */
uint64_t vgic_int_get_act(struct vcpu *vcpu, struct vgic_int *interrupt)
{
    return (interrupt->state & ACT) ? 1 : 0;
}

/**
 * @brief Set the cfg (level/edge sensitivity) for the vgic interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @param cfg cfg value
 * @return true if the update modified the interrupt state, false otherwise
 */
bool vgic_int_set_cfg(struct vcpu *vcpu, struct vgic_int *interrupt, uint64_t cfg)
{
    uint8_t prev_cfg = interrupt->cfg;
    interrupt->cfg = (uint8_t)cfg;
    return prev_cfg != cfg;
}

/**
 * @brief Read the cfg of a given vgic interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @return uint64_t the cfg value for the interrupt
 */
uint64_t vgic_int_get_cfg(struct vcpu *vcpu, struct vgic_int *interrupt)
{
    return (uint64_t)interrupt->cfg;
}

/**
 * @brief Commit the interrupt cfg to the real physical interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 */
void vgic_int_set_cfg_hw(struct vcpu *vcpu, struct vgic_int *interrupt)
{
#if (GIC_VERSION != GICV2)
    if (gic_is_priv(interrupt->id)) {
        gicr_set_icfgr(interrupt->id, interrupt->cfg, interrupt->phys.redist);
    } else {
        gicd_set_icfgr(interrupt->id, interrupt->cfg);
    }
#else
    gic_set_icfgr(interrupt->id, interrupt->cfg);
#endif
}

/**
 * @brief Set the priority of a given vgic interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @param prio the target priority value
 * @return true if the priority was updated, false otherwise
 */
bool vgic_int_set_prio(struct vcpu *vcpu, struct vgic_int *interrupt, uint64_t prio)
{
    uint8_t prev_prio = interrupt->prio;
    interrupt->prio = (uint8_t)prio &
        BIT_MASK(8-GICH_LR_PRIO_LEN, GICH_LR_PRIO_LEN);
    return prev_prio != prio;
}

/**
 * @brief Read the priority of a given vgic interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @return uint64_t the priority value
 */
uint64_t vgic_int_get_prio(struct vcpu *vcpu, struct vgic_int *interrupt)
{
    return (uint64_t)interrupt->prio;
}


/**
 * @brief Commit the interrupt priority to the real physical interrupt
 * 
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 */
void vgic_int_set_prio_hw(struct vcpu *vcpu, struct vgic_int *interrupt)
{
#if (GIC_VERSION != GICV2)
    if (gic_is_priv(interrupt->id)) {
        gicr_set_prio(interrupt->id, interrupt->prio, interrupt->phys.redist);
    } else {
        gicd_set_prio(interrupt->id, interrupt->prio);
    }
#else
    gic_set_prio(interrupt->id, interrupt->prio);
#endif
}

/**
 * @brief Emulates the access to a gic raz/wi (read-as-zero/write ignore) 
 * register.
 * 
 * @param acc pointer to a emulation struct containing information about the
 * guest access
 * @param handlers pointer to a vgic register access handling information structure
 * @param gicr_access boolean indicating if this is an access to a 
 * redistributor (must be false)
 * @param vgicr_id the gicr id in case this is a gicr access (ignored)
 */
void vgic_emul_razwi(struct emul_access *acc, struct vgic_reg_handler_info *handlers,
                     bool gicr_access, cpuid_t vgicr_id)
{
    if (!acc->write) vcpu_writereg(cpu.vcpu, acc->reg, 0);
}

/**
 * @brief Set a generic field of a GIC configuration using specific callbacks
 * defined in the handlers structure
 * 
 * @param handlers pointer to a vgic register access handling information structure
 * @param vcpu a pointer to the vcpu part of the vm associated with the vgic
 * @param interrupt pointer to the interrupt struct
 * @param data generic data to be interpreted only by the invoked field-specific
 * handlers
 * 
 * @note as cpu messages are sent in this function, it implies the
 * allocation of a message node from a slab down the line.
 */
void vgic_int_set_field(struct vgic_reg_handler_info *handlers, struct vcpu *vcpu,
                        struct vgic_int *interrupt, uint64_t data)
{
    spin_lock(&interrupt->lock);
    if (vgic_get_ownership(vcpu, interrupt)) {
        vgic_remove_lr(vcpu, interrupt);
        if (handlers->update_field(vcpu, interrupt, data) &&
            vgic_int_is_hw(interrupt)) {
            handlers->update_hw(vcpu, interrupt);
        }
        vgic_route(vcpu, interrupt);
        vgic_yield_ownership(vcpu, interrupt);
    } else {
        struct cpu_msg msg = {VGIC_IPI_ID, VGIC_SET_REG,
                         VGIC_MSG_DATA(vcpu->vm->id, 0, interrupt->id,
                                       handlers->regid, data)};
        cpu_send_msg(interrupt->owner->phys_id, &msg);
    }
    spin_unlock(&interrupt->lock);
}

/**
 * @brief Generic emulation handler for accessing vgic configuration registers 
 * with multiple interrupts per register.
 * 
 * @param acc pointer to a emulation struct containing information about the
 * guest access
 * @param handlers pointer to a vgic register access handling information structure
 * @param gicr_access boolean indiciating if this was a vgicr access
 * @param vgicr_id in case gicr is true, contains the vgicr id, otherwise, 
 * it is ignored
 */
void vgic_emul_generic_access(struct emul_access *acc,
                              struct vgic_reg_handler_info *handlers,
                              bool gicr_access, cpuid_t vgicr_id)
{
    size_t field_width = handlers->field_width;
    size_t first_int =
        (GICD_REG_MASK(acc->addr) - handlers->regroup_base) * 8 / field_width;
    uint64_t val = acc->write ? vcpu_readreg(cpu.vcpu, acc->reg) : 0;
    uint64_t mask = (1ull << field_width) - 1;
    bool valid_access =
        (GIC_VERSION == GICV2) || !(gicr_access ^ gic_is_priv(first_int));

    if (valid_access) {
        for (size_t i = 0; i < ((acc->width * 8) / field_width); i++) {
            struct vgic_int *interrupt =
                vgic_get_int(cpu.vcpu, first_int + i, vgicr_id);
            if (interrupt == NULL) break;
            if (acc->write) {
                uint64_t data = bit64_extract(val, i * field_width, field_width);
                vgic_int_set_field(handlers, cpu.vcpu, interrupt, data);
            } else {
                val |= (handlers->read_field(cpu.vcpu, interrupt) & mask)
                       << (i * field_width);
            }
        }
    }

    if (!acc->write) {
        vcpu_writereg(cpu.vcpu, acc->reg, val);
    }
}

struct vgic_reg_handler_info isenabler_info = {
    vgic_emul_generic_access,
    0b0100,
    VGIC_ISENABLER_ID,
    offsetof(struct gicd_hw, ISENABLER),
    1,
    vgic_int_get_enable,
    vgic_int_set_enable,
    vgic_int_enable_hw,
};

struct vgic_reg_handler_info ispendr_info = {
    vgic_emul_generic_access,
    0b0100,
    VGIC_ISPENDR_ID,
    offsetof(struct gicd_hw, ISPENDR),
    1,
    vgic_int_get_pend,
    vgic_int_set_pend,
    vgic_int_state_hw,
};

struct vgic_reg_handler_info isactiver_info = {
    vgic_emul_generic_access,
    0b0100,
    VGIC_ISACTIVER_ID,
    offsetof(struct gicd_hw, ISACTIVER),
    1,
    vgic_int_get_act,
    vgic_int_set_act,
    vgic_int_state_hw,
};

struct vgic_reg_handler_info icenabler_info = {
    vgic_emul_generic_access,
    0b0100,
    VGIC_ICENABLER_ID,
    offsetof(struct gicd_hw, ICENABLER),
    1,
    vgic_int_get_enable,
    vgic_int_clear_enable,
    vgic_int_enable_hw,
};

struct vgic_reg_handler_info icpendr_info = {
    vgic_emul_generic_access,
    0b0100,
    VGIC_ICPENDR_ID,
    offsetof(struct gicd_hw, ICPENDR),
    1,
    vgic_int_get_pend,
    vgic_int_clear_pend,
    vgic_int_state_hw,
};

struct vgic_reg_handler_info iactiver_info = {
    vgic_emul_generic_access,
    0b0100,
    VGIC_ICACTIVER_ID,
    offsetof(struct gicd_hw, ICACTIVER),
    1,
    vgic_int_get_act,
    vgic_int_clear_act,
    vgic_int_state_hw,
};

struct vgic_reg_handler_info icfgr_info = {
    vgic_emul_generic_access,
    0b0100,
    VGIC_ICFGR_ID,
    offsetof(struct gicd_hw, ICFGR),
    2,
    vgic_int_get_cfg,
    vgic_int_set_cfg,
    vgic_int_set_cfg_hw,
};

struct vgic_reg_handler_info ipriorityr_info = {
    vgic_emul_generic_access,
    0b0101,
    VGIC_IPRIORITYR_ID,
    offsetof(struct gicd_hw, IPRIORITYR),
    8,
    vgic_int_get_prio,
    vgic_int_set_prio,
    vgic_int_set_prio_hw,
};

struct vgic_reg_handler_info vgicd_misc_info = {
    vgicd_emul_misc_access,
    0b0100,
};

struct vgic_reg_handler_info vgicd_pidr_info = {
    vgicd_emul_pidr_access,
    0b0100,
};

struct vgic_reg_handler_info razwi_info = {
    vgic_emul_razwi,
    0b0100,
};

__attribute__((weak)) struct vgic_reg_handler_info itargetr_info = {
    vgic_emul_razwi,
    0b0101,
};

__attribute__((weak)) struct vgic_reg_handler_info sgir_info = {
    vgic_emul_razwi,
    0b0100,
};

__attribute__((weak)) struct vgic_reg_handler_info irouter_info = {
    vgic_emul_razwi,
    0b0100,
};

struct vgic_reg_handler_info *reg_handler_info_table[VGIC_REG_HANDLER_ID_NUM] =
    {[VGIC_ISENABLER_ID] = &isenabler_info,
     [VGIC_ISPENDR_ID] = &ispendr_info,
     [VGIC_ISACTIVER_ID] = &isactiver_info,
     [VGIC_ICENABLER_ID] = &icenabler_info,
     [VGIC_ICPENDR_ID] = &icpendr_info,
     [VGIC_ICACTIVER_ID] = &iactiver_info,
     [VGIC_ICFGR_ID] = &icfgr_info,
     [VGIC_IROUTER_ID] = &irouter_info,
     [VGIC_IPRIORITYR_ID] = &ipriorityr_info,
     [VGIC_ITARGETSR_ID] = &itargetr_info};

struct vgic_reg_handler_info 
    *vgic_get_reg_handler_info(enum vgic_reg_handler_info_id id)
{
    if (id < VGIC_REG_HANDLER_ID_NUM) {
        return reg_handler_info_table[id];
    } else {
        return NULL;
    }
}

/**
 * @brief Check if the agligment of the access to a given vgic register is
 * valid.
 * 
 * @param acc pointer to a emulation struct containing information about the
 * guest access
 * @param handlers pointer to a vgic register access handling information structure
 * @return true if the access alignment is legal, false otherwise
 */
bool vgic_check_reg_alignment(struct emul_access *acc,
                              struct vgic_reg_handler_info *handlers)
{
    if (!(handlers->alignment & acc->width) ||
        ((acc->addr & (acc->width - 1)) != 0)) {
        return false;
    } else {
        return true;
    }
}

/**
 * @brief First level emulation handler for the virtual gic distributor. 
 * 
 * Detects which register is being accessed retrieving the specific handling
 * information. If the access aligment is valid, it calls the specific 
 * register emulation handler
 * 
 * @param acc pointer to a emulation struct containing information about the
 * guest access
 */
bool vgicd_emul_handler(struct emul_access *acc)
{
    struct vgic_reg_handler_info *handler_info = NULL;
    switch (GICD_REG_MASK(acc->addr) >> 7) {
        case GICD_REG_GROUP(CTLR):
            handler_info = &vgicd_misc_info;
            break;
        case GICD_REG_GROUP(ISENABLER):
            handler_info = &isenabler_info;
            break;
        case GICD_REG_GROUP(ISPENDR):
            handler_info = &ispendr_info;
            break;
        case GICD_REG_GROUP(ISACTIVER):
            handler_info = &isactiver_info;
            break;
        case GICD_REG_GROUP(ICENABLER):
            handler_info = &icenabler_info;
            break;
        case GICD_REG_GROUP(ICPENDR):
            handler_info = &icpendr_info;
            break;
        case GICD_REG_GROUP(ICACTIVER):
            handler_info = &iactiver_info;
            break;
        case GICD_REG_GROUP(ICFGR):
            handler_info = &icfgr_info;
            break;
        case GICD_REG_GROUP(SGIR):
            handler_info = &sgir_info;
            break;
        default: {
            size_t acc_off = GICD_REG_MASK(acc->addr);
            if (GICD_IS_REG(IPRIORITYR, acc_off)) {
                handler_info = &ipriorityr_info;
            } else if (GICD_IS_REG(ITARGETSR, acc_off)) {
                handler_info = &itargetr_info;
            } else if (GICD_IS_REG(IROUTER, acc_off)) {
                handler_info = &irouter_info;
            } else if (GICD_IS_REG(ID, acc_off)) {
                handler_info = &vgicd_pidr_info;
            } else {
                handler_info = &razwi_info;
            }
        }
    }

    if (vgic_check_reg_alignment(acc, handler_info)) {
        spin_lock(&cpu.vcpu->vm->arch.vgicd.lock);
        handler_info->reg_access(acc, handler_info, false, cpu.vcpu->id);
        spin_unlock(&cpu.vcpu->vm->arch.vgicd.lock);
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Inject an interrupt tied to a physical interrupt in vgic associated
 * with the vcpu
 * 
 * @param vcpu pointer to the vcpu associated to the vgic where the interrupt
 * is to be injected
 * @param id the id of the interrupt
 * 
 * @note this function is a fast path for injecting virtual interrupts tied 
 * to real physical interrupts
 */
void vgic_inject_hw(struct vcpu* vcpu, irqid_t id) {
    struct vgic_int *interrupt = vgic_get_int(vcpu, id, vcpu->id);
    spin_lock(&interrupt->lock);
    interrupt->owner = vcpu;
    interrupt->state = PEND;
    interrupt->in_lr = false;
    vgic_add_lr(vcpu, interrupt);
    spin_unlock(&interrupt->lock);
}

/**
 * @brief Inject an interrupt in the vgic associated with the vcpu
 * 
 * @param vcpu pointer to the vcpu associated to the vgic where the interrupt
 * is to be injected
 * @param id the id of the interrupt
 * @param source the source vcpu in case this is a gicv2 sgi
 */
void vgic_inject(struct vcpu* vcpu, irqid_t id, vcpuid_t source)
{
    struct vgic_int *interrupt = vgic_get_int(vcpu, id, vcpu->id);
    if (interrupt != NULL) {
        if (vgic_int_is_hw(interrupt)) {
            vgic_inject_hw(vcpu, id);
        } else if (GIC_VERSION == GICV2 && gic_is_sgi(id)) {
            vgic_inject_sgi(vcpu, interrupt, source);
        } else {
            vgic_int_set_field(&ispendr_info, vcpu, interrupt, true);
        }
    }
}

void vgic_local_route(struct vcpu *vcpu, irqid_t int_id) {
    struct vgic_int *interrupt =
        vgic_get_int(vcpu, int_id, vcpu->id);
    if (interrupt != NULL) {
        spin_lock(&interrupt->lock);
        if (vgic_get_ownership(vcpu, interrupt)) {
            if (vgic_int_vcpu_is_target(vcpu, interrupt)) {
                vgic_add_lr(vcpu, interrupt);
            }
            vgic_yield_ownership(vcpu, interrupt);
        }
        spin_unlock(&interrupt->lock);
    }
}

void vgic_set_reg(struct vcpu *vcpu, size_t vgicr_id, irqid_t int_id, size_t reg_id, uint64_t val) {
    struct vgic_reg_handler_info *handlers =
        vgic_get_reg_handler_info(reg_id);
    struct vgic_int *interrupt = vgic_get_int(vcpu, int_id, vgicr_id);
    if (handlers != NULL && interrupt != NULL) {
        vgic_int_set_field(handlers, vcpu, interrupt, val);
    }
}

/**
 * @brief Handle IPIs related to vgic events.
 * 
 * @param event the id of the event that
 * @param data data containing information about the vm, vgic, interrupt
 * with additional event-specific data
 */
void vgic_ipi_handler(uint32_t event, uint64_t data)
{
    uint16_t vm_id = VGIC_MSG_VM(data);
    uint16_t vgicr_id = VGIC_MSG_VGICRID(data);
    irqid_t int_id = VGIC_MSG_INTID(data);
    uint64_t val = VGIC_MSG_VAL(data);

    if (vm_id != cpu.vcpu->vm->id) {
        ERROR("received vgic3 msg target to another vcpu");
        // TODO: need to fetch vcpu from other vm if the taget vm for this
        // is not active
    }

    switch (event) {
        case VGIC_UPDATE_ENABLE: {
            vgic_update_enable(cpu.vcpu);
        } break;

        case VGIC_ROUTE: {
            vgic_local_route(cpu.vcpu, int_id);
        } break;

        case VGIC_INJECT: {
            vgic_inject(cpu.vcpu, int_id, val);
        } break;

        case VGIC_SET_REG: {
            vgic_set_reg(cpu.vcpu, vgicr_id, int_id, VGIC_MSG_REG(data), val);
        } break;
    }
}

/**
 * Must be called holding the vgic_spilled_lock
 */

/**
 * @brief Find the highest priority spilled interrupt that can be injected
 * in the vcpu's vgic
 * 
 * @param vcpu the vcpu associated with the vgic
 * @param flags contain the desired state (active and/or pending of the 
 * interrupt) of the wanted interrupt
 * @param[out] outlist the spilled list from where the interrupt is saved (
 * can be vcpu private or global to the vm)
 * @return struct vgic_int* a pointer to the found interrupt, NULL if no
 * interrupt was found
 *
 */
static inline 
struct vgic_int* vgic_highest_prio_spilled(struct vcpu *vcpu, 
                                           unsigned flags, 
                                           struct list** outlist) {
    struct vgic_int* irq = NULL;
    struct list* spilled_lists[] = {
        &vcpu->arch.vgic_spilled,
        &vcpu->vm->arch.vgic_spilled,
    };
    size_t spilled_list_size = sizeof(spilled_lists)/sizeof(struct list*);
    for(size_t i = 0; i< spilled_list_size; i++) {
        struct list *list = spilled_lists[i];
        list_foreach((*list), struct vgic_int, temp_irq) {
            if(!(vgic_get_state(temp_irq) & flags)) { continue; }
            bool irq_is_null = irq == NULL;
            uint8_t irq_prio = irq_is_null ? GIC_LOWEST_PRIO : irq->prio;
            irqid_t irq_id = irq_is_null ? GIC_MAX_VALID_INTERRUPTS : irq->id;
            bool is_higher_prio = (temp_irq->prio < irq_prio);
            bool is_same_prio = temp_irq->prio == irq_prio;
            bool is_lower_id = temp_irq->id < irq_id;
            if (is_higher_prio || (is_same_prio && is_lower_id)) {
                irq = temp_irq;
                *outlist = list;
            }
        }
    }
    return irq;
}

/**
 * @brief Fill all free list registers with spilled interrupts.
 * 
 * @param vcpu 
 * @param npie true if this function was called due to an NPIE maintenance 
 * interrupt
 */
static void vgic_refill_lrs(struct vcpu *vcpu, bool npie) {
    uint64_t elrsr = gich_get_elrsr();
    ssize_t  lr_ind = bitmap_find_nth((bitmap_t*)&elrsr, NUM_LRS, 1, 0, true);
    unsigned flags = npie ? PEND : ACT | PEND;
    spin_lock(&vcpu->vm->arch.vgic_spilled_lock);
    while(lr_ind >= 0) {
        struct list* list = NULL;
        struct vgic_int* irq = vgic_highest_prio_spilled(vcpu, flags, &list);
        if (irq != NULL) {
            spin_lock(&irq->lock);
            bool got_ownership = vgic_get_ownership(vcpu, irq);
            if(got_ownership) {
                list_rm(list, &irq->node);
                vgic_write_lr(vcpu, irq, lr_ind);
            }
            spin_unlock(&irq->lock);
            if(!got_ownership) { continue; }
        } else {
            uint32_t hcr = gich_get_hcr();
            gich_set_hcr(hcr & ~(GICH_HCR_NPIE_BIT | GICH_HCR_UIE_BIT));
            break;
        }
        flags = ACT | PEND;
        elrsr = gich_get_elrsr();
        lr_ind = bitmap_find_nth((bitmap_t*)&elrsr, NUM_LRS, 1, 0, true);
    }
    spin_unlock(&vcpu->vm->arch.vgic_spilled_lock);
}

/**
 * @brief Emulate GICC.EOIR access (clearing the active state) 
 * for the highest priority spilled interrupt.
 * 
 * This function is called due to an LRENP maintanence interrupt that arises 
 * due to the VM accessing EOIR for an interrupt that spilled.
 * 
 * @param vcpu pointer to the vcpu that performed the EOIR access
 */
static void vgic_eoir_highest_spilled_active(struct vcpu *vcpu)
{   
    struct list* list = NULL;
    struct vgic_int *interrupt = 
        vgic_highest_prio_spilled(vcpu, ACT, &list);

    if (interrupt != NULL) {
        spin_lock(&interrupt->lock);
        if(vgic_get_ownership(vcpu, interrupt)) {
            interrupt->state &= ~ACT;
            if (vgic_int_is_hw(interrupt)) {
                gic_set_act(interrupt->id, false);
            } else {
                if (interrupt->state & PEND) {
                    vgic_add_lr(vcpu, interrupt);
                }
            }
        }
        spin_unlock(&interrupt->lock);
    }
}

/**
 * @brief Emulate GICC.EOIR access (clearing the active state) 
 * for an interrupt in a list register.
 *
 * This function is called due to an EOIR maintenance interrupt.
 * 
 * @param vcpu pointer to the vcpu that performed the EOIR access
 */
void vgic_handle_trapped_eoir(struct vcpu *vcpu)
{
    uint64_t eisr = gich_get_eisr();
    int64_t lr_ind = bitmap_find_nth((bitmap_t*)&eisr, NUM_LRS, 1, 0, true);
    while (lr_ind >= 0) {
        unsigned long lr_val = gich_read_lr(lr_ind);
        gich_write_lr(lr_ind, 0);

        struct vgic_int *interrupt =
            vgic_get_int(vcpu, GICH_LR_VID(lr_val), vcpu->id);
        if (interrupt == NULL) continue;

        spin_lock(&interrupt->lock);
        interrupt->in_lr = false;
        if (interrupt->id < GIC_MAX_SGIS) {
            vgic_add_lr(vcpu, interrupt);
        } else {
            vgic_yield_ownership(vcpu, interrupt);
        }
        spin_unlock(&interrupt->lock);
        eisr = gich_get_eisr();
        lr_ind = bitmap_find_nth((bitmap_t*)&eisr, NUM_LRS, 1, 0, true);
    }
}

/**
 * @brief The high level GIC maintence interrupt handler.
 * 
 * It checks which maintenance interrupt sources are asserted and calls
 * the corresponding handlers.
 * 
 * @param irq_id the ID of the interrupt that trigered the handler. It must
 * be the maintenance interrupt id, therefore it is ignored.
 */
void gic_maintenance_handler(irqid_t irq_id)
{
    uint32_t misr = gich_get_misr();

    if (misr & GICH_MISR_EOI) {
        vgic_handle_trapped_eoir(cpu.vcpu);
    }

    if (misr & (GICH_MISR_NP | GICH_MISR_U)) {
        vgic_refill_lrs(cpu.vcpu, !!(misr & GICH_MISR_NP));
    }

    if (misr & GICH_MISR_LRPEN) {
        uint32_t hcr_el2 = gich_get_hcr();
        while (hcr_el2 & GICH_HCR_EOICount_MASK) {
            vgic_eoir_highest_spilled_active(cpu.vcpu);
            hcr_el2 -= (1U << GICH_HCR_EOICount_OFF);
            gich_set_hcr(hcr_el2);
            hcr_el2 = gich_get_hcr();
        }
    }
}

/**
 * @brief Get number of interrupts in the vgic according to the vm configuration
 * 
 * @param gic_dscrp a pointer to the gic descriptor structure in the vm 
 * configuration
 * @return size_t the number of interrupts in the vgic
 */
size_t vgic_get_itln(const struct gic_dscrp *gic_dscrp) {

    /**
     * By default the guest sees the real platforms interrupt line number
     * in the virtual gic. However a user can control this using the 
     * interrupt_num in the platform description configuration which be at
     * least the number os ppis and a multiple of 32.
     */

    size_t vtyper_itln =
        bit32_extract(gicd.TYPER, GICD_TYPER_ITLN_OFF, GICD_TYPER_ITLN_LEN);

    if(gic_dscrp->interrupt_num > GIC_MAX_PPIS) {
        vtyper_itln = (ALIGN(gic_dscrp->interrupt_num, 32)/32 - 1) & 
            BIT32_MASK(0, GICD_TYPER_ITLN_LEN);
    }

    return vtyper_itln;
}

/**
 * @brief Mark a given interrupt as an hardware interrupt in for the vgic
 * associated with the vm
 * 
 * @param vm a pointer to the target vm structure
 * @param id the interrupt id
 */
void vgic_set_hw(struct vm *vm, irqid_t id)
{
    if (id < GIC_MAX_SGIS) return;

    struct vgic_int *interrupt = NULL;

    if (id < GIC_CPU_PRIV) {
        list_foreach(vm->vcpu_list, struct vcpu, vcpu)
        {
            interrupt = vgic_get_int(vcpu, id, vcpu->id);
            if (interrupt != NULL) {
                spin_lock(&interrupt->lock);
                interrupt->hw = true;
                spin_unlock(&interrupt->lock);
            }
        }
    } else {
        /**
         * This assumes this method is called only during VM initlization
         */
        interrupt = vgic_get_int((struct vcpu *)list_peek(&vm->vcpu_list), id, 0);
        if (interrupt != NULL) {
            spin_lock(&interrupt->lock);
            interrupt->hw = true;
            spin_unlock(&interrupt->lock);
        } else {
            WARNING("trying to link non-existent virtual irq to physical irq")
        }
    }
}
