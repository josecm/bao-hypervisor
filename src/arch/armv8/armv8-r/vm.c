/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <vm.h>
#include <config.h>
#include <arch/sysregs.h>
#include <arch/fences.h>

static size_t vcpu_mpu_num_regions(void)
{
    return (sysreg_mpuir_el1_read() >> MPUIR_EL1_REGIONS_OFF) & MPUIR_EL1_REGIONS_MSK;
}

void vcpu_arch_profile_init(struct vcpu* vcpu, struct vm* vm)
{
    UNUSED_ARG(vm);

    mpu_struct_init(&vcpu->arch.profile.mpu);
}

void vcpu_profile_reset(struct vcpu* vcpu)
{
    size_t mpu_num_regions = vcpu_mpu_num_regions();
    for (size_t i = 0; i < mpu_num_regions; i++) {
        vcpu->arch.profile.mpu_el1.prbar[i] = 0;
    }

    vcpu->arch.profile.mpu_el1.prselr = 0;
}

void vm_arch_profile_init(struct vm* vm)
{
    sysreg_vsctlr_el2_write((vm->id << VSCTLR_EL2_VMID_OFF) & VSCTLR_EL2_VMID_MSK);

    if (DEFINED(MEM_PROT_MPU) && DEFINED(AARCH64) && vm->config->platform.mmu) {
        unsigned long vtcr = VTCR_MSA;
        sysreg_vtcr_el2_write(vtcr);
    }
}

#if defined AARCH32

// For the only known armv8-r aarch32 implementation (cortex-52) the number
// of implemented el1 mpu regions must be 16, 20 or 24

static void vcpu_el1_mpu_save_state(struct vcpu* vcpu)
{
    vcpu->arch.profile.mpu_el1.prselr = sysreg_prselr_el1_read();

    switch (vcpu_mpu_num_regions()) {
        case 32:
            vcpu->arch.profile.mpu_el1.prbar[31] = sysreg_prbar31_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[31] = sysreg_prlar31_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[30] = sysreg_prbar30_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[30] = sysreg_prlar30_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[29] = sysreg_prbar29_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[29] = sysreg_prlar29_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[28] = sysreg_prbar28_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[28] = sysreg_prlar28_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[27] = sysreg_prbar27_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[27] = sysreg_prlar27_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[26] = sysreg_prbar26_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[26] = sysreg_prlar26_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[25] = sysreg_prbar25_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[25] = sysreg_prlar25_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[24] = sysreg_prbar24_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[24] = sysreg_prlar24_el1_read();
            __attribute__((fallthrough));
        case 24:
            vcpu->arch.profile.mpu_el1.prbar[23] = sysreg_prbar23_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[23] = sysreg_prlar23_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[22] = sysreg_prbar22_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[22] = sysreg_prlar22_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[21] = sysreg_prbar21_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[21] = sysreg_prlar21_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[20] = sysreg_prbar20_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[20] = sysreg_prlar20_el1_read();
            __attribute__((fallthrough));
        case 20:
            vcpu->arch.profile.mpu_el1.prbar[19] = sysreg_prbar19_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[19] = sysreg_prlar19_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[18] = sysreg_prbar18_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[18] = sysreg_prlar18_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[17] = sysreg_prbar17_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[17] = sysreg_prlar17_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[16] = sysreg_prbar16_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[16] = sysreg_prlar16_el1_read();
            __attribute__((fallthrough));
        case 16:
            vcpu->arch.profile.mpu_el1.prbar[15] = sysreg_prbar15_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[15] = sysreg_prlar15_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[14] = sysreg_prbar14_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[14] = sysreg_prlar14_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[13] = sysreg_prbar13_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[13] = sysreg_prlar13_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[12] = sysreg_prbar12_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[12] = sysreg_prlar12_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[11] = sysreg_prbar11_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[11] = sysreg_prlar11_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[10] = sysreg_prbar10_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[10] = sysreg_prlar10_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[9] = sysreg_prbar9_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[9] = sysreg_prlar9_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[8] = sysreg_prbar8_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[8] = sysreg_prlar8_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[7] = sysreg_prbar7_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[7] = sysreg_prlar7_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[6] = sysreg_prbar6_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[6] = sysreg_prlar6_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[5] = sysreg_prbar5_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[5] = sysreg_prlar5_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[4] = sysreg_prbar4_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[4] = sysreg_prlar4_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[3] = sysreg_prbar3_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[3] = sysreg_prlar3_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[2] = sysreg_prbar2_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[2] = sysreg_prlar2_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[1] = sysreg_prbar1_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[1] = sysreg_prlar1_el1_read();
            vcpu->arch.profile.mpu_el1.prbar[0] = sysreg_prbar0_el1_read();
            vcpu->arch.profile.mpu_el1.prlar[0] = sysreg_prlar0_el1_read();
            break;
        default:
            ERROR("PLAT_NUM_EL1_MPU_REGIONS must be 16, 20 or 24");
    }
}

static void vcpu_el1_mpu_restore_state(struct vcpu* vcpu)
{
    sysreg_prselr_el1_write(vcpu->arch.profile.mpu_el1.prselr);

    switch (vcpu_mpu_num_regions()) {
        case 32:
            sysreg_prbar31_el1_write(vcpu->arch.profile.mpu_el1.prbar[31]);
            sysreg_prlar31_el1_write(vcpu->arch.profile.mpu_el1.prlar[31]);
            sysreg_prbar30_el1_write(vcpu->arch.profile.mpu_el1.prbar[30]);
            sysreg_prlar30_el1_write(vcpu->arch.profile.mpu_el1.prlar[30]);
            sysreg_prbar29_el1_write(vcpu->arch.profile.mpu_el1.prbar[29]);
            sysreg_prlar29_el1_write(vcpu->arch.profile.mpu_el1.prlar[29]);
            sysreg_prbar28_el1_write(vcpu->arch.profile.mpu_el1.prbar[28]);
            sysreg_prlar28_el1_write(vcpu->arch.profile.mpu_el1.prlar[28]);
            sysreg_prbar27_el1_write(vcpu->arch.profile.mpu_el1.prbar[27]);
            sysreg_prlar27_el1_write(vcpu->arch.profile.mpu_el1.prlar[27]);
            sysreg_prbar26_el1_write(vcpu->arch.profile.mpu_el1.prbar[26]);
            sysreg_prlar26_el1_write(vcpu->arch.profile.mpu_el1.prlar[26]);
            sysreg_prbar25_el1_write(vcpu->arch.profile.mpu_el1.prbar[25]);
            sysreg_prlar25_el1_write(vcpu->arch.profile.mpu_el1.prlar[25]);
            sysreg_prbar24_el1_write(vcpu->arch.profile.mpu_el1.prbar[24]);
            sysreg_prlar24_el1_write(vcpu->arch.profile.mpu_el1.prlar[24]);
            __attribute__((fallthrough));
        case 24:
            sysreg_prbar23_el1_write(vcpu->arch.profile.mpu_el1.prbar[23]);
            sysreg_prlar23_el1_write(vcpu->arch.profile.mpu_el1.prlar[23]);
            sysreg_prbar22_el1_write(vcpu->arch.profile.mpu_el1.prbar[22]);
            sysreg_prlar22_el1_write(vcpu->arch.profile.mpu_el1.prlar[22]);
            sysreg_prbar21_el1_write(vcpu->arch.profile.mpu_el1.prbar[21]);
            sysreg_prlar21_el1_write(vcpu->arch.profile.mpu_el1.prlar[21]);
            sysreg_prbar20_el1_write(vcpu->arch.profile.mpu_el1.prbar[20]);
            sysreg_prlar20_el1_write(vcpu->arch.profile.mpu_el1.prlar[20]);
            __attribute__((fallthrough));
        case 20:
            sysreg_prbar19_el1_write(vcpu->arch.profile.mpu_el1.prbar[19]);
            sysreg_prlar19_el1_write(vcpu->arch.profile.mpu_el1.prlar[19]);
            sysreg_prbar18_el1_write(vcpu->arch.profile.mpu_el1.prbar[18]);
            sysreg_prlar18_el1_write(vcpu->arch.profile.mpu_el1.prlar[18]);
            sysreg_prbar17_el1_write(vcpu->arch.profile.mpu_el1.prbar[17]);
            sysreg_prlar17_el1_write(vcpu->arch.profile.mpu_el1.prlar[17]);
            sysreg_prbar16_el1_write(vcpu->arch.profile.mpu_el1.prbar[16]);
            sysreg_prlar16_el1_write(vcpu->arch.profile.mpu_el1.prlar[16]);
            __attribute__((fallthrough));
        case 16:
            sysreg_prbar15_el1_write(vcpu->arch.profile.mpu_el1.prbar[15]);
            sysreg_prlar15_el1_write(vcpu->arch.profile.mpu_el1.prlar[15]);
            sysreg_prbar14_el1_write(vcpu->arch.profile.mpu_el1.prbar[14]);
            sysreg_prlar14_el1_write(vcpu->arch.profile.mpu_el1.prlar[14]);
            sysreg_prbar13_el1_write(vcpu->arch.profile.mpu_el1.prbar[13]);
            sysreg_prlar13_el1_write(vcpu->arch.profile.mpu_el1.prlar[13]);
            sysreg_prbar12_el1_write(vcpu->arch.profile.mpu_el1.prbar[12]);
            sysreg_prlar12_el1_write(vcpu->arch.profile.mpu_el1.prlar[12]);
            sysreg_prbar11_el1_write(vcpu->arch.profile.mpu_el1.prbar[11]);
            sysreg_prlar11_el1_write(vcpu->arch.profile.mpu_el1.prlar[11]);
            sysreg_prbar10_el1_write(vcpu->arch.profile.mpu_el1.prbar[10]);
            sysreg_prlar10_el1_write(vcpu->arch.profile.mpu_el1.prlar[10]);
            sysreg_prbar9_el1_write(vcpu->arch.profile.mpu_el1.prbar[9]);
            sysreg_prlar9_el1_write(vcpu->arch.profile.mpu_el1.prlar[9]);
            sysreg_prbar8_el1_write(vcpu->arch.profile.mpu_el1.prbar[8]);
            sysreg_prlar8_el1_write(vcpu->arch.profile.mpu_el1.prlar[8]);
            sysreg_prbar7_el1_write(vcpu->arch.profile.mpu_el1.prbar[7]);
            sysreg_prlar7_el1_write(vcpu->arch.profile.mpu_el1.prlar[7]);
            sysreg_prbar6_el1_write(vcpu->arch.profile.mpu_el1.prbar[6]);
            sysreg_prlar6_el1_write(vcpu->arch.profile.mpu_el1.prlar[6]);
            sysreg_prbar5_el1_write(vcpu->arch.profile.mpu_el1.prbar[5]);
            sysreg_prlar5_el1_write(vcpu->arch.profile.mpu_el1.prlar[5]);
            sysreg_prbar4_el1_write(vcpu->arch.profile.mpu_el1.prbar[4]);
            sysreg_prlar4_el1_write(vcpu->arch.profile.mpu_el1.prlar[4]);
            sysreg_prbar3_el1_write(vcpu->arch.profile.mpu_el1.prbar[3]);
            sysreg_prlar3_el1_write(vcpu->arch.profile.mpu_el1.prlar[3]);
            sysreg_prbar2_el1_write(vcpu->arch.profile.mpu_el1.prbar[2]);
            sysreg_prlar2_el1_write(vcpu->arch.profile.mpu_el1.prlar[2]);
            sysreg_prbar1_el1_write(vcpu->arch.profile.mpu_el1.prbar[1]);
            sysreg_prlar1_el1_write(vcpu->arch.profile.mpu_el1.prlar[1]);
            sysreg_prbar0_el1_write(vcpu->arch.profile.mpu_el1.prbar[0]);
            sysreg_prlar0_el1_write(vcpu->arch.profile.mpu_el1.prlar[0]);
            break;
        default:
            ERROR("PLAT_NUM_EL1_MPU_REGIONS must be 16, 20 or 24");
    }

}

#elif defined AARCH64


// For the only known armv8-r aarch32 implementation (cortex-52) the number
// of implemented el1 mpu regions must be 16 or 32

static void vcpu_el1_mpu_save_state(struct vcpu* vcpu)
{
    vcpu->arch.profile.mpu_el1.prselr = sysreg_prselr_el1_read();

    sysreg_prselr_el1_write(0);
    ISB();
    vcpu->arch.profile.mpu_el1.prbar[15] = sysreg_prbar15_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[15] = sysreg_prlar15_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[14] = sysreg_prbar14_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[14] = sysreg_prlar14_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[13] = sysreg_prbar13_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[13] = sysreg_prlar13_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[12] = sysreg_prbar12_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[12] = sysreg_prlar12_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[11] = sysreg_prbar11_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[11] = sysreg_prlar11_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[10] = sysreg_prbar10_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[10] = sysreg_prlar10_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[9] = sysreg_prbar9_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[9] = sysreg_prlar9_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[8] = sysreg_prbar8_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[8] = sysreg_prlar8_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[7] = sysreg_prbar7_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[7] = sysreg_prlar7_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[6] = sysreg_prbar6_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[6] = sysreg_prlar6_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[5] = sysreg_prbar5_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[5] = sysreg_prlar5_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[4] = sysreg_prbar4_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[4] = sysreg_prlar4_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[3] = sysreg_prbar3_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[3] = sysreg_prlar3_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[2] = sysreg_prbar2_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[2] = sysreg_prlar2_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[1] = sysreg_prbar1_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[1] = sysreg_prlar1_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[0] = sysreg_prbar0_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[0] = sysreg_prlar0_el1_read();


    if (vcpu_mpu_num_regions() >= 16) {
        sysreg_prselr_el1_write(1);
        ISB();
        vcpu->arch.profile.mpu_el1.prbar[31] = sysreg_prbar15_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[31] = sysreg_prlar15_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[30] = sysreg_prbar14_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[30] = sysreg_prlar14_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[29] = sysreg_prbar13_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[29] = sysreg_prlar13_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[28] = sysreg_prbar12_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[28] = sysreg_prlar12_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[27] = sysreg_prbar11_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[27] = sysreg_prlar11_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[26] = sysreg_prbar10_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[26] = sysreg_prlar10_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[25] = sysreg_prbar9_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[25] = sysreg_prlar9_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[24] = sysreg_prbar8_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[24] = sysreg_prlar8_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[23] = sysreg_prbar7_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[23] = sysreg_prlar7_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[22] = sysreg_prbar6_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[22] = sysreg_prlar6_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[21] = sysreg_prbar5_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[21] = sysreg_prlar5_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[20] = sysreg_prbar4_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[20] = sysreg_prlar4_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[19] = sysreg_prbar3_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[19] = sysreg_prlar3_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[18] = sysreg_prbar2_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[18] = sysreg_prlar2_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[17] = sysreg_prbar1_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[17] = sysreg_prlar1_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[16] = sysreg_prbar0_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[16] = sysreg_prlar0_el1_read();
    }

    sysreg_prselr_el1_write(vcpu->arch.profile.mpu_el1.prselr);
}

static void vcpu_el1_mpu_restore_state(struct vcpu* vcpu)
{   
    sysreg_prselr_el1_write(0);
    ISB();
    vcpu->arch.profile.mpu_el1.prlar[0] = sysreg_prlar0_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[0] = sysreg_prbar0_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[1] = sysreg_prbar1_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[1] = sysreg_prlar1_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[2] = sysreg_prbar2_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[2] = sysreg_prlar2_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[3] = sysreg_prbar3_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[3] = sysreg_prlar3_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[4] = sysreg_prbar4_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[4] = sysreg_prlar4_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[5] = sysreg_prbar5_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[5] = sysreg_prlar5_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[6] = sysreg_prbar6_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[6] = sysreg_prlar6_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[7] = sysreg_prbar7_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[7] = sysreg_prlar7_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[8] = sysreg_prbar8_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[8] = sysreg_prlar8_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[9] = sysreg_prbar9_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[9] = sysreg_prlar9_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[10] = sysreg_prbar10_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[10] = sysreg_prlar10_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[11] = sysreg_prbar11_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[11] = sysreg_prlar11_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[12] = sysreg_prbar12_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[12] = sysreg_prlar12_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[13] = sysreg_prbar13_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[13] = sysreg_prlar13_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[14] = sysreg_prbar14_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[14] = sysreg_prlar14_el1_read();
    vcpu->arch.profile.mpu_el1.prbar[15] = sysreg_prbar15_el1_read();
    vcpu->arch.profile.mpu_el1.prlar[15] = sysreg_prlar15_el1_read();
    
    if (vcpu_mpu_num_regions() >= 16) {
        sysreg_prselr_el1_write(1);
        ISB();
        vcpu->arch.profile.mpu_el1.prlar[16] = sysreg_prlar0_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[16] = sysreg_prbar0_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[17] = sysreg_prbar1_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[17] = sysreg_prlar1_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[18] = sysreg_prbar2_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[18] = sysreg_prlar2_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[19] = sysreg_prbar3_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[19] = sysreg_prlar3_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[20] = sysreg_prbar4_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[20] = sysreg_prlar4_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[21] = sysreg_prbar5_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[21] = sysreg_prlar5_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[22] = sysreg_prbar6_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[22] = sysreg_prlar6_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[23] = sysreg_prbar7_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[23] = sysreg_prlar7_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[24] = sysreg_prbar8_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[24] = sysreg_prlar8_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[25] = sysreg_prbar9_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[25] = sysreg_prlar9_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[26] = sysreg_prbar10_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[26] = sysreg_prlar10_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[27] = sysreg_prbar11_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[27] = sysreg_prlar11_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[28] = sysreg_prbar12_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[28] = sysreg_prlar12_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[29] = sysreg_prbar13_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[29] = sysreg_prlar13_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[30] = sysreg_prbar14_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[30] = sysreg_prlar14_el1_read();
        vcpu->arch.profile.mpu_el1.prbar[31] = sysreg_prbar15_el1_read();
        vcpu->arch.profile.mpu_el1.prlar[31] = sysreg_prlar15_el1_read();
    }

    sysreg_prselr_el1_write(vcpu->arch.profile.mpu_el1.prselr);
}

#endif

void vcpu_arch_profile_save_state(struct vcpu* vcpu)
{
    vcpu_el1_mpu_save_state(vcpu);
}

void vcpu_arch_profile_restore_state(struct vcpu* vcpu)
{
    vcpu_el1_mpu_restore_state(vcpu);
    mpu_restore(&vcpu->arch.profile.mpu);
}
