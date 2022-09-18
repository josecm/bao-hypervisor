/**
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <mem.h>
#include <cpu.h>
#include <arch/sysregs.h>
#include <mem_prot/mem.h>

#define PRIO_ORDER          NO_PRIORITY
#define MP_GRANULARITY      PAGE_SIZE

struct memory_protection_dscr{
    unsigned long entries;
    unsigned long granularity;
    unsigned long priority_order;
}mp;

bool mem_translate(struct addr_space* as, vaddr_t va, paddr_t* pa)
{
    return false;
}

static inline void mem_set_memory_struct()
{
    mp.granularity = MP_GRANULARITY;
    mp.priority_order = PRIO_ORDER;
    mp.entries = (sysreg_hmpuir_read() & HMPUIR_REGIONS);
}

void as_arch_init(struct addr_space* as)
{
    if (cpu()->id == CPU_MASTER){
        mem_set_memory_struct();
    }
    as->mem_prot_desc = &mp;
}

unsigned long mem_get_granularity()
{
    return PAGE_SIZE;
}

unsigned long mem_get_mp_entries()
{
    return cpu()->as.mem_prot_desc->entries;
}

void mem_read_physical_entry(struct memory_protection *mp_entry, unsigned long region)
{
    unsigned long base = 0, lim = 0;
    sysreg_hprselr_write(region);
    base = sysreg_hprbar_read();
    lim = sysreg_hprlar_read();
    if(lim & 0x1) mp_entry->assigned = true;
        else mp_entry->assigned = false; 
    mp_entry->mem_flags = (HPRBAR_CONF(base) | (HPRLAR_CONF(lim)<<4));
    mp_entry->base_addr = (base & PRBAR_BASE_MSK);
    mp_entry->limit_addr = (lim |(0x3F));
}

static inline mpid_t get_region_num(paddr_t addr)
{
    ssize_t reg_num = 0;
    struct memory_protection region;
    while(bitmap_get(cpu()->arch.profile.mem_p, reg_num) ||
            reg_num<cpu()->as.mem_prot_desc->entries)
    {
        mem_read_physical_entry(&region, reg_num);
        if(addr >= region.base_addr || addr <= region.limit_addr) break;
        reg_num++;
    }
    if (reg_num<cpu()->as.mem_prot_desc->entries) reg_num = -1;
    return reg_num;
}

mpid_t get_available_physical_region()
{
    mpid_t reg_num = 0;
    unsigned long status = 0;
    while(reg_num<cpu()->as.mem_prot_desc->entries && !status)
    {
        if (bitmap_get(cpu()->arch.profile.mem_p, reg_num) == 0) status = 1;
        reg_num++;
    }
    if (!status) reg_num = -1;
    return (--reg_num);
}

void mem_free_physical_region(paddr_t addr)
{
    mpid_t reg_num = get_region_num(addr);
    bitmap_clear(cpu()->arch.profile.mem_p, reg_num);
    sysreg_hprselr_write(reg_num);
    sysreg_hprlar_write(0);
    sysreg_hprbar_write(0);
}

void mem_write_mp(paddr_t pa, size_t n, mem_flags_t flags)
{
    unsigned long lim = (pa+n);
    lim = (lim - 1) & PRLAR_LIMIT_MSK;
    mpid_t reg = get_available_physical_region();
    if (reg == -1) ERROR("No available MPU regions!");
    bitmap_set(cpu()->arch.profile.mem_p, reg);
    sysreg_hprselr_write(reg);
    sysreg_hprbar_write(PRBAR_BASE(pa) | HPRBAR_CONF(flags));
    sysreg_hprlar_write(PRLAR_LIMIT(lim) | HPRLAR_CONF(flags) | ENABLE_MASK);
}
