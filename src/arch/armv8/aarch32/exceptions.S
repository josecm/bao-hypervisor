/** 
 * Bao, a Lightweight Static Partitioning Hypervisor 
 *
 * Copyright (c) Bao Project (www.bao-project.org), 2019-
 *
 * Authors:
 *      Sandro Pinto <sandro@bao-project.org>
 *      Afonso Santos <afomms@gmail.com>
 *
 * Bao is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 2 as published by the Free
 * Software Foundation, with a special exception exempting guest code from such
 * license. See the COPYING file in the top-level directory for details. 
 *
 */

#include <asm_defs.h>
#include <arch/sysregs.h>

#define ENTRY_SIZE   (0x4)

.text 

.macro SAVE_HYP_GPRS

    sub sp, sp, #(14*4)
    push {r0-r12}

.endm

.macro VM_EXIT

    push {r0-r12}
    SAVE_SP_LR
    SAVE_ELR_SPSR

.endm

.macro VM_ENTRY

    mcr p15, 4, r0, c13, c0, 2  // Read HTPIDR (CPU base address)
    ldr r0, [r0, CPU_VCPU_OFF]
    add r0, r0, VCPU_REGS_OFF
    mov sp, r0
    LOAD_ELR_SPSR
    LOAD_SP_LR
    pop {r0-r12}
    eret
    b   .

.endm

.macro SAVE_ELR_SPSR

    mrs r0, ELR_hyp
    mrs r1, SPSR_hyp
    push {r0, r1}

.endm

.macro LOAD_ELR_SPSR

    pop {r0, r1}
    msr ELR_hyp, r0
    msr SPSR_hyp, r1

.endm

.macro SET_SP

    mcr p15, 4, r0, c13, c0, 2  // Read HTPIDR (CPU base address)
    add r0, r0, CPU_STACK_OFF
    add r0, r0, CPU_STACK_SIZE
    mov sp, r0
    
.endm

.macro SAVE_SP_LR

    mrs r3, SPSR_hyp
    and r3, r3, #0x1F
    
    cmp r3, #SPSR_USR
    beq 1f
    cmp r3, #SPSR_IRQ
    beq 2f
    cmp r3, #SPSR_SVC
    beq 3f
    cmp r3, #SPSR_ABT
    beq 4f
    cmp r3, #SPSR_UND
    beq 5f
    
    1:  //read_usr:
        mrs r0,  sp_usr
        mrs r1,  lr_usr
        b 6f
    2:  //read_irq:
        mrs r0,  sp_irq
        mrs r1,  lr_irq
        b 6f
    3:  //read_hvc:
        mrs r0,  sp_svc
        mrs r1,  lr_svc
        b 6f
    4:  //read_abt:
        mrs r0,  sp_abt
        mrs r1,  lr_abt
        b 6f
    5:  //read_und:
        mrs r0,  sp_und
        mrs r1,  lr_und
        b 6f
    6:
        push {r0-r1}

.endm

.macro LOAD_SP_LR

    and r1, r1, #0x1F
    pop {r2-r3}
    
    cmp r1, #SPSR_USR
    beq 1f
    cmp r1, #SPSR_IRQ
    beq 2f
    cmp r1, #SPSR_SVC
    beq 3f
    cmp r1, #SPSR_ABT
    beq 4f
    cmp r1, #SPSR_UND
    beq 5f
    
    1:  //write_usr
        msr sp_usr, r2
        msr lr_usr, r3
        b 6f
    2:  //write_irq
        msr sp_irq, r2
        msr lr_irq, r3
        b 6f
    3:  //write_hvc
        msr sp_svc, r2
        msr lr_svc, r3
        b 6f
    4:  //write_abt
        msr sp_abt, r2
        msr lr_abt, r3
        b 6f
    5:  //write_und
        msr sp_und, r2
        msr lr_und, r3
        b 6f
    6:

.endm


.balign 0x20
.global _hyp_vector_table
_hyp_vector_table:
    b	.
    b	hyp_und_handler
    b	hyp_hvc_handler
    b	hyp_fetch_abt_handler
    b	hyp_data_abt_handler
    b	hyp_trap_handler
    b   hyp_irq_handler
    b   hyp_fiq_handler


.global hyp_und_handler
hyp_und_handler:
    b	.  


.global hyp_hvc_handler
hyp_hvc_handler:
    b	. 


.global hyp_fetch_abt_handler
hyp_fetch_abt_handler:
    SAVE_HYP_GPRS
    mov r0, sp
    bl internal_abort_handler
    b .


.global hyp_data_abt_handler
hyp_data_abt_handler:
    SAVE_HYP_GPRS
    mov r0, sp
    bl internal_abort_handler
    b .


.global hyp_trap_handler
hyp_trap_handler:
    VM_EXIT
    SET_SP
    bl	aborts_sync_handler
    VM_ENTRY


.global hyp_irq_handler
hyp_irq_handler:
    VM_EXIT
    SET_SP
    bl  gic_handle
    VM_ENTRY


.global hyp_fiq_handler
hyp_fiq_handler:
    b   .


.global vcpu_arch_entry
vcpu_arch_entry:
    VM_ENTRY
