/**
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <cpu.h>
#include <platform.h>
#include <arch/gic.h>
#include <bitmap.h>

extern struct cpu_synctoken cpu_mem_sync;
bitmap_t cpus_bm;

void cpu_arch_profile_init(cpuid_t cpuid, paddr_t load_addr) {
    /*  Enable Interrupt Controller to send ipi during memory initialization */
    sysreg_icc_sre_el2_write(ICC_SRE_SRE_BIT | ICC_SRE_ENB_BIT);
    
    if (cpuid == CPU_MASTER){
        cpu_sync_init(&cpu_mem_sync, (platform.cpu_num-1));
    }
}

void cpu_mem_prot_bitmap_init(struct cpu_arch_profile* mp)
{
    if (PLAT_MP_ENTRIES > mem_get_mp_entries())
    {
        ERROR("Defined more entries on memory protection than available on hardware");
    }
    bitmap_clear_consecutive(mp->mem_p, 0, mem_get_mp_entries());
}

void cpu_arch_profile_idle() {
    asm volatile("wfi");
}

void cpu_sync_memprot()
{
    cpu_sync_barrier(&cpu_glb_sync);
    if (cpu()->id != CPU_MASTER) {
        cpu_msg_handler();
        cpu_sync_barrier(&cpu_mem_sync);
    }
}

void cpu_broadcast_init(struct addr_space *as)
{
    /* When a CPU broadcast a region it is for all other CPUs*/
    bitmap_set_consecutive(&cpus_bm, 0, PLAT_CPU_NUM);
    as->cpus = cpus_bm;
}
