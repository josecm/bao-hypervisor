/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <arch/mpu.h>

#include <mem.h>
#include <cpu.h>
#include <vm.h>
#include <arch/sysregs.h>
#include <arch/fences.h>
#include <objpool.h>
#include <config.h>
#include <bitmap.h>

static inline struct mpu* mpu_find_by_as(struct addr_space* as) {

    struct mpu* mpu = NULL;

    if (as->type == AS_VM) {
        list_foreach(cpu()->vcpu_list, struct vcpu, vcpu) {
            if (vcpu->vm->as.id == as->id) {
                mpu = &vcpu->arch.profile.mpu;
                break;
            }
        }
    } else {
        mpu = &cpu()->arch.profile.mpu;
    }

    return mpu;
}

static unsigned long mpu_get_region_base(struct mpu* mpu, mpid_t mpid)
{
    return PRBAR_BASE(mpu->entry[mpid].prbar);
}

static mpid_t mpu_find_region(struct mpu* mpu, struct mp_region* mpr)
{
    mpid_t mpid = INVALID_MPID;

    for (mpid_t i = 0; i < MPU_NUM_ENTRIES; i++) {
        if (bit_get(mpu->prenr, i)) {
            if (mpu_get_region_base(mpu, i) == mpr->base) {
                mpid = i;
                break;
            }
        }
    }
    return mpid;
}

static mpid_t mpu_entry_allocate(struct mpu* mpu, bool locked)
{
    size_t count_min = ~((size_t)0);
    mpid_t mpid = INVALID_MPID;
    for (size_t i = 0; i < MPU_NUM_ENTRIES; i++) {
        
        bool entry_used_by_mpu = bit_get(mpu->prenr, i);
        size_t entry_locked = bit_get(cpu()->arch.profile.mpu_mngmnt.locked_entries, i);
        if (entry_used_by_mpu || entry_locked) {
            continue;
        }

        size_t alloc_count = cpu()->arch.profile.mpu_mngmnt.entry_allocation_count[i];
        if (locked) { 
            if (alloc_count == 0) {
                mpid = i;
                break;
            }
        } else {
            if (alloc_count < count_min) {
                mpid = i;
                count_min = alloc_count;
            }
        }
    }

    if (mpid != INVALID_MPID) {
        if (locked) {
            cpu()->arch.profile.mpu_mngmnt.locked_entries = bit_set(cpu()->arch.profile.mpu_mngmnt.locked_entries, mpid);
        }
        cpu()->arch.profile.mpu_mngmnt.entry_allocation_count[mpid] += 1;
    }

    return mpid;
}

static inline void mpu_entry_deallocate(struct mpu* mpu, mpid_t mpid)
{
    UNUSED_ARG(mpu);
    cpu()->arch.profile.mpu_mngmnt.entry_allocation_count[mpid] -= 1;
    cpu()->arch.profile.mpu_mngmnt.locked_entries = bit_clear(cpu()->arch.profile.mpu_mngmnt.locked_entries, mpid);
}

static void mpu_entry_set(struct mpu* mpu, mpid_t mpid, struct mp_region* mpr)
{
    unsigned long lim = mpr->base + mpr->size - 1;

    mpu->entry[mpid].prbar = (mpr->base & PRBAR_BASE_MSK) | mpr->mem_flags.prbar;
    mpu->entry[mpid].prlar = (lim & PRLAR_LIMIT_MSK) | mpr->mem_flags.prlar;

    if (mpu->active) {
        sysreg_prselr_el2_write(mpid);
        ISB();
        sysreg_prbar_el2_write(mpu->entry[mpid].prbar);
        sysreg_prlar_el2_write(mpu->entry[mpid].prlar);
        DSB(nsh);
        ISB();
    }
}

static void mpu_entry_update_limit(struct mpu* mpu, mpid_t mpid, struct mp_region* mpr)
{
    unsigned long lim = mpr->base + mpr->size - 1;
    mpu->entry[mpid].prlar = ((lim & PRLAR_LIMIT_MSK) | mpr->mem_flags.prlar);

    if (mpu->active) {
        sysreg_prselr_el2_write(mpid);
        ISB();
        sysreg_prlar_el2_write(mpu->entry[mpid].prlar);
        DSB(nsh);
        ISB();
    }
}

static bool mpu_entry_clear(struct mpu* mpu, mpid_t mpid)
{   
    mpu->entry[mpid].prbar = 0;
    mpu->entry[mpid].prlar = 0;

    if (mpu->active) {
        sysreg_prselr_el2_write(mpid);
        ISB();
        sysreg_prlar_el2_write(0);
        sysreg_prbar_el2_write(0);
        DSB(nsh);
        ISB();
    }

    return true;
}

bool mpu_map(struct addr_space* as, struct mp_region* mpr, bool locked)
{
    struct mpu* mpu = mpu_find_by_as(as);
    bool region_mapped = false;
    mpid_t mpid = INVALID_MPID;

    /* We don't check if there is an existing region because bao ensure that
    there is only 1 address space active at the same time, and as such, only a
    single set of mpu entries are enabled.
    Furthermore, the same check is done at the vMPU level.
    */

    if ((mpu != NULL) && (mpr->size > 0)) {
        mpid = mpu_entry_allocate(mpu, locked);
        if (mpid != INVALID_MPID) {
            mpu_entry_set(mpu, mpid, mpr);
            mpu->prenr = bit_set(mpu->prenr, mpid);
            mpu->first_entry = (mpid_t)bit_ffs(mpu->prenr);
            if (locked) {
                mpu->locked = bit_set(mpu->locked, mpid);
            }
            region_mapped = true;
        }
    }

    return region_mapped;
}

bool mpu_unmap(struct addr_space* as, struct mp_region* mpr)
{
    struct mpu* mpu = mpu_find_by_as(as);
    bool region_unmapped = false;

    if (mpu != NULL) {
        mpid_t mpid = mpu_find_region(mpu, mpr);

        if (mpid != INVALID_MPID) {
            mpu_entry_deallocate(mpu, mpid);
            mpu_entry_clear(mpu, mpid);
            mpu->prenr = bit_clear(mpu->prenr, mpid);
            if (mpu->prenr == 0) {
                mpu->first_entry = INVALID_MPID;
            } else {
                mpu->first_entry = (mpid_t)bit_ffs(mpu->prenr);
            }
            mpu->locked = bit_clear(mpu->locked, mpid);
            region_unmapped = true;
        }
    }

    return region_unmapped;
}

bool mpu_update(struct addr_space* as, struct mp_region* mpr)
{
    struct mpu* mpu = mpu_find_by_as(as);

    if (mpu != NULL) {
        mpid_t mpid = mpu_find_region(mpu, mpr);

        if (mpid != INVALID_MPID) {
            mpu_entry_update_limit(mpu, mpid, mpr);
            return true;
        }
    }

    return false;
}

bool mpu_perms_compatible(unsigned long perms1, unsigned long perms2)
{
    return perms1 == perms2;
}

void mpu_struct_init(struct mpu* mpu)
{
    mpu->active = false;
    mpu->resident = false;
    mpu->first_entry = INVALID_MPID;
    mpu->locked = 0;
    mpu->prenr = 0;

    for (size_t i = 0; i < MPU_NUM_ENTRIES; i++) {
        mpu->entry[i].prbar = 0;
        mpu->entry[i].prlar = 0;
    }
}

void mpu_enable(void)
{
    unsigned long reg_val;

    reg_val = sysreg_sctlr_el2_read();
    reg_val |= (SCTLR_M | SCTLR_C);
    sysreg_sctlr_el2_write(reg_val);

    ISB();
}

void mpu_init()
{
    cpu()->arch.profile.mpu_mngmnt.locked_entries = 0;
    cpu()->arch.profile.mpu_mngmnt.active_guest_mpu = NULL;

    for (mpid_t mpid = 0; mpid < MPU_NUM_ENTRIES; mpid++) {
        cpu()->arch.profile.mpu_mngmnt.entry_allocation_count[mpid] = 0;
        cpu()->arch.profile.mpu_mngmnt.resident[mpid] = NULL;
    }

    mpu_struct_init(&cpu()->arch.profile.mpu);
    cpu()->arch.profile.mpu.active = true;
}

void mpu_restore(struct mpu* mpu)
{
    if (!mpu->resident && (mpu->first_entry != INVALID_MPID)) {
        unsigned long prenr = mpu->prenr >> mpu->first_entry;
        for (mpid_t mpid = mpu->first_entry; (mpid < MPU_NUM_ENTRIES) && (prenr != 0); mpid++) {
            if (bit_get(prenr, 0)) {
                if (cpu()->arch.profile.mpu_mngmnt.resident[mpid] != NULL) {
                    *cpu()->arch.profile.mpu_mngmnt.resident[mpid] = false;
                }
                cpu()->arch.profile.mpu_mngmnt.resident[mpid] = &mpu->resident;
                
                sysreg_prselr_el2_write(mpid);
                ISB();
                sysreg_prbar_el2_write(mpu->entry[mpid].prbar);
                sysreg_prlar_el2_write(mpu->entry[mpid].prlar);
                DSB(nsh);
                ISB();

            }
            prenr >>= 1;
        }
        mpu->resident = true;
    }

    if (cpu()->arch.profile.mpu_mngmnt.active_guest_mpu != NULL) {
        cpu()->arch.profile.mpu_mngmnt.active_guest_mpu->active = false;
    }
    cpu()->arch.profile.mpu_mngmnt.active_guest_mpu = mpu;

    mpu->active = true;
}
