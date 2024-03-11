/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <arch/psci.h>
#include <arch/sysregs.h>
#include <fences.h>
#include <vm.h>
#include <cpu.h>
#include <mem.h>
#include <cache.h>
#include <config.h>

enum { PSCI_MSG_ON };

union psci_msg_data {
    struct {
        uint16_t vm_id;
    };
    uint64_t raw;
};

#define PSCI_MSG_DATA(VM_ID)                                      \
    (union psci_msg_data)                                         \
    {                                                             \
        .vm_id = (uint16_t)VM_ID,                                 \
    }

/* --------------------------------
    SMC Trapping
--------------------------------- */

void psci_wake_from_off(vmid_t vm_id)
{
    struct vcpu* vcpu = cpu_get_vcpu_by_vmid(vm_id);

    if (vcpu == NULL) {
        WARNING("Trying to wake up non-existent vcpu");
        return;
    }

    /* update vcpu()->psci_ctx */
    spin_lock(&vcpu->arch.psci_ctx.lock);
    if (vcpu->arch.psci_ctx.state == ON_PENDING) {
        vcpu_arch_reset(vcpu, vcpu->arch.psci_ctx.entrypoint);
        vcpu->arch.psci_ctx.state = ON;
        vcpu_writereg(vcpu, 0, vcpu->arch.psci_ctx.context_id);
    }
    spin_unlock(&vcpu->arch.psci_ctx.lock);
}

static void psci_cpumsg_handler(uint32_t event, uint64_t data)
{
    union psci_msg_data msg = { .raw = data };
    vmid_t vm_id = msg.vm_id;

    switch (event) {
        case PSCI_MSG_ON:
            psci_wake_from_off(vm_id);
            break;
        default:
            WARNING("Unknown PSCI IPI event");
            break;
    }
}

CPU_MSG_HANDLER(psci_cpumsg_handler, PSCI_CPUMSG_ID)

static int32_t psci_cpu_suspend_handler(struct vcpu* vcpu)
{

    uint32_t power_state = (uint32_t)vcpu_readreg(vcpu, 1);
    unsigned long entrypoint = vcpu_readreg(vcpu, 2);
    unsigned long context_id = vcpu_readreg(vcpu, 3);

    /**
     * !! Ignoring the rest of the requested  powerstate for now. This might be a problem howwver
     * since powerlevel and stateid are implementation defined.
     */
    uint32_t state_type = power_state & PSCI_STATE_TYPE_BIT;
    int32_t ret;

    if (state_type) {
        // PSCI_STATE_TYPE_POWERDOWN:
        spin_lock(&vcpu->arch.psci_ctx.lock);
        vcpu->arch.psci_ctx.entrypoint = entrypoint;
        vcpu->arch.psci_ctx.context_id = context_id;
        spin_unlock(&vcpu->arch.psci_ctx.lock);
        ret = psci_power_down();
    } else {
        // PSCI_STATE_TYPE_STANDBY:
        ret = psci_standby();
    }

    return ret;
}

static int32_t psci_cpu_off_handler(struct vcpu* vcpu)
{
    /**
     *  Right now we only support one vcpu por cpu, so passthrough the request directly to the
     *  monitor psci implementation. Later another vcpu, will call cpu_on on this vcpu()->
     */

    spin_lock(&vcpu->arch.psci_ctx.lock);
    vcpu->arch.psci_ctx.state = OFF;
    spin_unlock(&vcpu->arch.psci_ctx.lock);

    cpu_powerdown();

    spin_lock(&vcpu->arch.psci_ctx.lock);
    vcpu->arch.psci_ctx.state = ON;
    spin_unlock(&vcpu->arch.psci_ctx.lock);

    return PSCI_E_DENIED;
}

static int32_t psci_cpu_on_handler(struct vcpu* vcpu)
{
    int32_t ret;
    unsigned long target_cpu_mpidr = vcpu_readreg(vcpu, 1);
    unsigned long entrypoint = vcpu_readreg(vcpu, 2);
    unsigned long context_id = vcpu_readreg(vcpu, 3);
    struct vm* vm = vcpu->vm;
    struct vcpu* target_vcpu = vm_get_vcpu_by_mpidr(vm, target_cpu_mpidr);

    if (target_vcpu != NULL) {
        bool already_on = true;
        spin_lock(&vcpu->arch.psci_ctx.lock);
        if (target_vcpu->arch.psci_ctx.state == OFF) {
            target_vcpu->arch.psci_ctx.state = ON_PENDING;
            target_vcpu->arch.psci_ctx.entrypoint = entrypoint;
            target_vcpu->arch.psci_ctx.context_id = context_id;
            fence_sync_write();
            already_on = false;
        }
        spin_unlock(&vcpu->arch.psci_ctx.lock);

        if (already_on) {
            return PSCI_E_ALREADY_ON;
        }

        cpuid_t pcpuid = vm_translate_to_pcpuid(vm, target_vcpu->id);
        if (pcpuid == INVALID_CPUID) {
            ret = PSCI_E_INVALID_PARAMS;
        } else {
            struct cpu_msg msg = { (uint32_t)PSCI_CPUMSG_ID, PSCI_MSG_ON, PSCI_MSG_DATA(vcpu->vm->id).raw };
            cpu_send_msg(pcpuid, &msg);
            ret = PSCI_E_SUCCESS;
        }

    } else {
        ret = PSCI_E_INVALID_PARAMS;
    }

    return ret;
}

static int32_t psci_affinity_info_handler(struct vcpu* vcpu)
{
    /* return ON, if at least one core in the affinity instance: has been enabled with a call to
    CPU_ON, and that core has not called CPU_OFF */

    /* return off if all of the cores in the affinity instance have called CPU_OFF and each of
    these calls has been processed by the PSCI implementation. */

    /*  return ON_PENDING if at least one core in the affinity instance is in the ON_PENDING state
     */

    /**
     * TODO
     */

    UNUSED_ARG(vcpu);

    return 0;
}

static int32_t psci_features_handler(struct vcpu* vcpu)
{
    uint32_t feature_id = (uint32_t)vcpu_readreg(vcpu, 0);
    int32_t ret = PSCI_E_NOT_SUPPORTED;

    switch (feature_id) {
        case PSCI_VERSION:
        case PSCI_CPU_OFF:
        case PSCI_CPU_SUSPEND_SMC32:
        case PSCI_CPU_SUSPEND_SMC64:
        case PSCI_CPU_ON_SMC32:
        case PSCI_CPU_ON_SMC64:
        case PSCI_AFFINITY_INFO_SMC32:
        case PSCI_AFFINITY_INFO_SMC64:
        case PSCI_FEATURES:
            ret = PSCI_E_SUCCESS;
            break;
        default:
            ret = PSCI_E_NOT_SUPPORTED;
            break;
    }

    return ret;
}

int32_t psci_smc_handler(struct vcpu* vcpu)
{
    int32_t ret = PSCI_E_NOT_SUPPORTED;
    unsigned long smc_fid = vcpu_readreg(vcpu, 0);

    switch (smc_fid) {
        case PSCI_VERSION:
            ret = PSCI_VERSION_0_2;
            break;

        case PSCI_CPU_OFF:
            ret = psci_cpu_off_handler(vcpu);
            break;

        case PSCI_CPU_SUSPEND_SMC32:
        case PSCI_CPU_SUSPEND_SMC64:
            ret = psci_cpu_suspend_handler(vcpu);
            break;

        case PSCI_CPU_ON_SMC32:
        case PSCI_CPU_ON_SMC64:
            ret = psci_cpu_on_handler(vcpu);
            break;

        case PSCI_AFFINITY_INFO_SMC32:
        case PSCI_AFFINITY_INFO_SMC64:
            ret = psci_affinity_info_handler(vcpu);
            break;

        case PSCI_FEATURES:
            ret = psci_features_handler(vcpu);
            break;

        case PSCI_MIG_INFO_TYPE:
            ret = PSCI_TOS_NOT_PRESENT_MP;
            break;

        default:
            INFO("unkown psci smc_fid 0x%lx", smc_fid);
    }

    return ret;
}
