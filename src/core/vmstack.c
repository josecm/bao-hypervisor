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

#include <hypercall.h>
#include <vm.h>
#include <cpu.h>
#include <string.h>
#include <timer.h>

void vmstack_unwind(struct vcpu* vcpu){
    
    if(!vcpu->stacked){
        return;
    }

    struct vcpu* temp_vcpu = NULL;

    do {
        node_t* vcpu_node = list_pop_head(&cpu()->vcpu_stack);
        temp_vcpu = CONTAINER_OF(struct vcpu, cpu_vcpu_stack_node, vcpu_node);
        temp_vcpu->stacked = false;
        timer_event_remove(&vcpu->stack_timer_event);
    } while(temp_vcpu != vcpu);

    cpu()->next_vcpu = vcpu;
}

static void vmstack_timer_event_handler(struct timer_event *timer) {
    struct vcpu* vcpu = (struct vcpu*)timer->data;
    vmstack_unwind(vcpu);
}

void vmstack_push(struct vcpu* vcpu){

    if(cpu()->vcpu != NULL){
        cpu()->vcpu->stacked = true;
        list_push_head(&cpu()->vcpu_stack, &cpu()->vcpu->cpu_vcpu_stack_node);
        struct timer_event *tmr_evt = vcpu_get_timer_event(cpu()->vcpu);
        if (tmr_evt != NULL) {
            tmr_evt->handler = vmstack_timer_event_handler;
            tmr_evt->data = cpu()->vcpu;
            timer_event_add(tmr_evt);
        }
    }
    
    cpu()->next_vcpu = vcpu;
}

struct vcpu* vmstack_pop(){

    node_t* vcpu_node = list_pop_head(&cpu()->vcpu_stack);
    struct vcpu* vcpu = NULL;

    if(vcpu_node != NULL){
        vcpu = CONTAINER_OF(struct vcpu, cpu_vcpu_stack_node, vcpu_node);
        vcpu->stacked = false;
        timer_event_remove(&vcpu->stack_timer_event);
        struct vcpu *temp = vcpu;
        vcpu = cpu()->vcpu;
        cpu()->next_vcpu = temp;
    }

    return vcpu;
}


enum {
    VMSTACK_RESUME = 1,
    VMSTACK_GOTO = 2,
    VMSTACK_RETURN = 3,
};

int64_t vmstack_hypercall(uint64_t id, uint64_t arg0, uint64_t arg1)
{
    int64_t res = HC_E_SUCCESS;

    struct vcpu* child = NULL;
    switch(id) {
        case VMSTACK_RESUME:
        case VMSTACK_GOTO:
            if((child = vcpu_get_child(cpu()->vcpu, arg0)) != NULL){
                if(id == VMSTACK_GOTO) {
                    vcpu_writepc(child, arg1);
                }
                vmstack_push(child);
            } else {
                res = -HC_E_INVAL_ARGS;
            }
            break;
        case VMSTACK_RETURN:
            vmstack_pop();
            break;
        default:
            res = -HC_E_FAILURE;
    }
    
    return res;
}
