/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <vm.h>
#include <arch/sysregs.h>

#define SPSR_INDEX_FIQ  (0)
#define SPSR_INDEX_IRQ  (1)
#define SPSR_INDEX_SVC  (2)
#define SPSR_INDEX_ABT  (3)
#define SPSR_INDEX_UND  (4)

static inline size_t spsr_mode_index(size_t spsr_m) {

    // TODO: detect not supported modes

    static const size_t mode_index_table[12] = {
         [1] = 0,   // 0b0001 = FIQ
         [2] = 1,   // 0b0010 = IRQ
         [3] = 2,   // 0b0011 = Supervisor
         [7] = 3,   // 0b0111 = Abort
         [11] = 4,  // 0b1011 = Undefined
    };

    return  mode_index_table[spsr_m & 0xf];
}

static inline uint32_t vcpu_spsr_m(struct vcpu* vcpu)
{
    return vcpu->regs.spsr_hyp & SPSR_M_MSK;
}

void vcpu_write_spsr_current_el1_mode(struct vcpu* vcpu, uint32_t spsr_val)
{
    uint32_t spsr_m = vcpu_spsr_m(vcpu);
    if (cpu_vcpu_is_current(vcpu)) {
        switch (spsr_m) {
            case SPSR_USR:
            case SPSR_SYS:
                /* No spsr for these modes */
                break;
            case SPSR_IRQ:
                sysreg_spsr_irq_write(spsr_val);
                break;
            case SPSR_FIQ:
                sysreg_spsr_fiq_write(spsr_val);
                break;
            case SPSR_SVC:
                sysreg_spsr_svc_write(spsr_val);
                break;
            case SPSR_ABT:
                sysreg_spsr_abt_write(spsr_val);
                break;
            case SPSR_UND:
                sysreg_spsr_und_write(spsr_val);
                break;
            default:
                ERROR("Got reserved mode from SPSR_HYP.M");
        }
    } else {
        vcpu->regs.spsr[spsr_mode_index(spsr_m)] = spsr_val;
    }
}


static inline uint32_t vcpu_read_sp_current_el1_mode(struct vcpu* vcpu)
{
    uint32_t spsr_m = vcpu_spsr_m(vcpu);
    uint32_t sp_val = 0;
    if (cpu_vcpu_is_current(vcpu)) {
        switch (spsr_m) {
            case SPSR_USR:
            case SPSR_SYS:
                sp_val = sysreg_sp_usr_read();
                break;
            case SPSR_IRQ:
                sp_val = sysreg_sp_irq_read();
                break;
            case SPSR_FIQ:
                sp_val = sysreg_sp_fiq_read();
                break;
            case SPSR_SVC:
                sp_val = sysreg_sp_svc_read();
                break;
            case SPSR_ABT:
                sp_val = sysreg_sp_abt_read();
                break;
            case SPSR_UND:
                sp_val = sysreg_sp_und_read();
                break;
            default:
                ERROR("Got reserved mode from SPSR_HYP.M");
        }
    } else {
        if (spsr_m == SPSR_USR || spsr_m == SPSR_SYS) {
            sp_val = vcpu->regs.x[13];
        } else {
            sp_val = vcpu->regs.sp[spsr_mode_index(spsr_m)];
        }
    }
        
    return sp_val;
}

static inline void vcpu_write_sp_current_el1_mode(struct vcpu* vcpu, uint32_t sp_val)
{
    uint32_t spsr_m = vcpu_spsr_m(vcpu);
    if (cpu_vcpu_is_current(vcpu)) {
        switch (spsr_m) {
            case SPSR_USR:
            case SPSR_SYS:
                sysreg_sp_usr_write(sp_val);
                break;
            case SPSR_IRQ:
                sysreg_sp_irq_write(sp_val);
                break;
            case SPSR_FIQ:
                sysreg_sp_fiq_write(sp_val);
                break;
            case SPSR_SVC:
                sysreg_sp_svc_write(sp_val);
                break;
            case SPSR_ABT:
                sysreg_sp_abt_write(sp_val);
                break;
            case SPSR_UND:
                sysreg_sp_und_write(sp_val);
                break;
            default:
                ERROR("Got reserved mode from SPSR_HYP.M");
        }
    } else {
        if (spsr_m == SPSR_USR || spsr_m == SPSR_SYS) {
            vcpu->regs.x[13] = sp_val;
        } else {
            vcpu->regs.sp[spsr_mode_index(spsr_m)] = sp_val;
        }
    }
}

static inline uint32_t vcpu_read_lr_current_el1_mode(struct vcpu* vcpu)
{
    uint32_t spsr_m = vcpu_spsr_m(vcpu);
    uint32_t lr_val = 0;
    if (cpu_vcpu_is_current(vcpu)) {
        switch (spsr_m) {
            case SPSR_USR:
            case SPSR_SYS:
                lr_val = vcpu->regs.lr_usr;
                break;
            case SPSR_IRQ:
                lr_val = sysreg_lr_irq_read();
                break;
            case SPSR_FIQ:
                lr_val = sysreg_lr_fiq_read();
                break;
            case SPSR_SVC:
                lr_val = sysreg_lr_svc_read();
                break;
            case SPSR_ABT:
                lr_val = sysreg_lr_abt_read();
                break;
            case SPSR_UND:
                lr_val = sysreg_lr_und_read();
                break;
            default:
                ERROR("Got reserved mode from SPSR_HYP.M");
        }
    } else {
        if (spsr_m == SPSR_USR || spsr_m == SPSR_SYS) {
            lr_val = vcpu->regs.lr_usr;
        } else {
            lr_val = vcpu->regs.lr[spsr_mode_index(spsr_m)];
        }
    }

    return lr_val;
}

inline void vcpu_write_lr_current_el1_mode(struct vcpu* vcpu, uint32_t lr_val)
{
    uint32_t spsr_m = vcpu_spsr_m(vcpu);
    if (cpu_vcpu_is_current(vcpu)) {
        switch (spsr_m) {
            case SPSR_USR:
            case SPSR_SYS:
                vcpu->regs.lr_usr = lr_val;
                break;
            case SPSR_IRQ:
                sysreg_lr_irq_write(lr_val);
                break;
            case SPSR_FIQ:
                sysreg_lr_fiq_write(lr_val);
                break;
            case SPSR_SVC:
                sysreg_lr_svc_write(lr_val);
                break;
            case SPSR_ABT:
                sysreg_lr_abt_write(lr_val);
                break;
            case SPSR_UND:
                sysreg_lr_und_write(lr_val);
                break;
            default:
                ERROR("Got reserved mode from SPSR_HYP.M");
        }
    }  else {
        if (spsr_m == SPSR_USR || spsr_m == SPSR_SYS) {
            vcpu->regs.lr_usr = lr_val;
        } else {
            vcpu->regs.lr[spsr_mode_index(spsr_m)] = lr_val;
        }
    }
}

static inline uint32_t vcpu_read_fiq_rx(struct vcpu* vcpu, unsigned long reg)
{
    uint32_t reg_val = 0;
    if (cpu_vcpu_is_current(vcpu)) {
        switch (reg) {
            case 8:
                reg_val = sysreg_r8_fiq_read();
                break;
            case 9:
                reg_val = sysreg_r9_fiq_read();
                break;
            case 10:
                reg_val = sysreg_r10_fiq_read();
                break;
            case 11:
                reg_val = sysreg_r11_fiq_read();
                break;
            case 12:
                reg_val = sysreg_r12_fiq_read();
                break;
            default:
                reg_val = 0;
        }
    } else {
        reg_val = vcpu->regs.fiq_rx[reg - 8];
    }

    return reg_val;
}

static inline void vcpu_write_fiq_rx(struct vcpu* vcpu, unsigned long reg,
    unsigned long reg_val)
{
    if (cpu_vcpu_is_current(vcpu)) {
        switch (reg) {
            case 8:
                sysreg_r8_fiq_write(reg_val);
                break;
            case 9:
                sysreg_r9_fiq_write(reg_val);
                break;
            case 10:
                sysreg_r10_fiq_write(reg_val);
                break;
            case 11:
                sysreg_r11_fiq_write(reg_val);
                break;
            case 12:
                sysreg_r12_fiq_write(reg_val);
                break;
            default:
                reg_val = 0;
        }
    } else {
        vcpu->regs.fiq_rx[reg - 8] = reg_val;
    }
}

unsigned long vcpu_readreg(struct vcpu* vcpu, unsigned long reg)
{
    uint32_t reg_val = 0;

    if (reg > 14) {
        reg_val = 0;
    } else if (reg == 13) {
        reg_val = vcpu_read_sp_current_el1_mode(vcpu);
    } else if (reg == 14) {
        reg_val = vcpu_read_lr_current_el1_mode(vcpu);
    } else if ((reg >= 8) && (vcpu_spsr_m(vcpu) == SPSR_FIQ)) {
        reg_val = vcpu_read_fiq_rx(vcpu, reg);
    } else {
        reg_val = vcpu->regs.x[reg];
    }

    return reg_val;
}

void vcpu_writereg(struct vcpu* vcpu, unsigned long reg, unsigned long val)
{
    if (reg > 14) {
        return;
    } else if (reg == 13) {
        vcpu_write_sp_current_el1_mode(vcpu, val);
    } else if (reg == 14) {
        vcpu_write_lr_current_el1_mode(vcpu, val);
    } else if ((reg >= 8) && (vcpu_spsr_m(vcpu) == SPSR_FIQ)) {
        vcpu_write_fiq_rx(vcpu, reg, val);
    } else {
        vcpu->regs.x[reg] = val;
    }
}

unsigned long vcpu_readpc(struct vcpu* vcpu)
{
    return vcpu->regs.elr_hyp;
}

void vcpu_writepc(struct vcpu* vcpu, unsigned long pc)
{
    vcpu->regs.elr_hyp = pc;
}

void vcpu_subarch_reset(struct vcpu* vcpu)
{
    vcpu->regs.spsr_hyp = SPSR_SVC | SPSR_F | SPSR_I | SPSR_A;
}

void vcpu_subarch_restore_state(struct vcpu *vcpu)
{
    sysreg_lr_fiq_write(vcpu->regs.lr[SPSR_INDEX_FIQ]);
    sysreg_lr_irq_write(vcpu->regs.lr[SPSR_INDEX_IRQ]);
    sysreg_lr_svc_write(vcpu->regs.lr[SPSR_INDEX_SVC]);
    sysreg_lr_abt_write(vcpu->regs.lr[SPSR_INDEX_ABT]);
    sysreg_lr_und_write(vcpu->regs.lr[SPSR_INDEX_UND]);

    sysreg_sp_fiq_write(vcpu->regs.sp[SPSR_INDEX_FIQ]);
    sysreg_sp_irq_write(vcpu->regs.sp[SPSR_INDEX_IRQ]);
    sysreg_sp_svc_write(vcpu->regs.sp[SPSR_INDEX_SVC]);
    sysreg_sp_abt_write(vcpu->regs.sp[SPSR_INDEX_ABT]);
    sysreg_sp_und_write(vcpu->regs.sp[SPSR_INDEX_UND]);

    sysreg_spsr_fiq_write(vcpu->regs.spsr[SPSR_INDEX_FIQ]);
    sysreg_spsr_irq_write(vcpu->regs.spsr[SPSR_INDEX_IRQ]);
    sysreg_spsr_svc_write(vcpu->regs.spsr[SPSR_INDEX_SVC]);
    sysreg_spsr_abt_write(vcpu->regs.spsr[SPSR_INDEX_ABT]);
    sysreg_spsr_und_write(vcpu->regs.spsr[SPSR_INDEX_UND]);

    sysreg_r8_fiq_write(vcpu->regs.fiq_rx[0]);
    sysreg_r9_fiq_write(vcpu->regs.fiq_rx[1]);
    sysreg_r10_fiq_write(vcpu->regs.fiq_rx[2]);
    sysreg_r11_fiq_write(vcpu->regs.fiq_rx[3]);
    sysreg_r12_fiq_write(vcpu->regs.fiq_rx[4]);

    sysreg_ifsr32_el2_write(vcpu->regs.ifsr32_el2);
}

void vcpu_subarch_save_state(struct vcpu* vcpu)
{
    vcpu->regs.lr[SPSR_INDEX_FIQ] = sysreg_lr_fiq_read();
    vcpu->regs.lr[SPSR_INDEX_IRQ] = sysreg_lr_irq_read();
    vcpu->regs.lr[SPSR_INDEX_SVC] = sysreg_lr_svc_read();
    vcpu->regs.lr[SPSR_INDEX_ABT] = sysreg_lr_abt_read();
    vcpu->regs.lr[SPSR_INDEX_UND] = sysreg_lr_und_read();
    
    vcpu->regs.sp[SPSR_INDEX_FIQ] = sysreg_sp_fiq_read();
    vcpu->regs.sp[SPSR_INDEX_IRQ] = sysreg_sp_irq_read();
    vcpu->regs.sp[SPSR_INDEX_SVC] = sysreg_sp_svc_read();
    vcpu->regs.sp[SPSR_INDEX_ABT] = sysreg_sp_abt_read();
    vcpu->regs.sp[SPSR_INDEX_UND] = sysreg_sp_und_read();

    vcpu->regs.spsr[SPSR_INDEX_FIQ] = sysreg_spsr_fiq_read();
    vcpu->regs.spsr[SPSR_INDEX_IRQ] = sysreg_spsr_irq_read();
    vcpu->regs.spsr[SPSR_INDEX_SVC] = sysreg_spsr_svc_read();
    vcpu->regs.spsr[SPSR_INDEX_ABT] = sysreg_spsr_abt_read();
    vcpu->regs.spsr[SPSR_INDEX_UND] = sysreg_spsr_und_read();

    vcpu->regs.fiq_rx[0] = sysreg_r8_fiq_read();
    vcpu->regs.fiq_rx[1] = sysreg_r9_fiq_read();
    vcpu->regs.fiq_rx[2] = sysreg_r10_fiq_read();
    vcpu->regs.fiq_rx[3] = sysreg_r11_fiq_read();
    vcpu->regs.fiq_rx[4] = sysreg_r12_fiq_read();

    vcpu->regs.ifsr32_el2 = sysreg_ifsr32_el2_read();
}
