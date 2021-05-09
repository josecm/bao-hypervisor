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

#include <vmstack.h>

#include <vm.h>
#include <cpu.h>
#include <string.h>

void vmstack_push(vcpu_t* vcpu){

    if(cpu.vcpu != NULL && vcpu->state != VCPU_INACTIVE){
        return;
    }

    if(cpu.vcpu != NULL){
        vcpu_save_state(cpu.vcpu);
        cpu.vcpu->state = VCPU_STACKED;
        list_push(&cpu.vcpu_stack, &cpu.vcpu->node);
        vcpu->parent = cpu.vcpu;
    }
    
    vcpu_restore_state(vcpu);
    vcpu->state = VCPU_ACTIVE;
    cpu.vcpu = vcpu;
}

vcpu_t* vmstack_pop(){

    vcpu_t* vcpu = (vcpu_t*) list_pop(&cpu.vcpu_stack);

    if(vcpu != NULL){
        vcpu->parent = NULL;
        vcpu_t *temp = vcpu;
        vcpu = cpu.vcpu;
        cpu.vcpu = temp;
        vcpu_save_state(vcpu);
        vcpu->state = VCPU_INACTIVE;
        vcpu_restore_state(cpu.vcpu);
        cpu.vcpu->state = VCPU_ACTIVE;
    }

    return vcpu;
}

void vmstack_unwind(vcpu_t* vcpu){
    
    if(vcpu->state != VCPU_STACKED){
        return;
    }

    vcpu_t* temp_vcpu = NULL;

    do {
        temp_vcpu = (vcpu_t*) list_pop(&cpu.vcpu_stack);
        temp_vcpu->state = VCPU_INACTIVE;
        temp_vcpu->parent = NULL;
    } while(temp_vcpu != vcpu);

    vcpu_save_state(cpu.vcpu);
    cpu.vcpu->state = VCPU_INACTIVE;
    cpu.vcpu = vcpu;
    vcpu_restore_state(cpu.vcpu);
    cpu.vcpu->state = VCPU_ACTIVE;
}