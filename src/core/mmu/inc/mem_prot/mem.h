/**
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef __MEM_PROT_H__
#define __MEM_PROT_H__

#include <bao.h>
#include <arch/mem.h>
#include <page_table.h>
#include <spinlock.h>
#include <bitmap.h>

enum AS_TYPE { AS_HYP = 0, AS_VM, AS_HYP_CPY };

#define HYP_ASID  0
struct addr_space {
    struct page_table pt;
    enum AS_TYPE type;
    bitmap_t cpus;
    colormap_t colors;
    asid_t id;
    spinlock_t lock;
};
enum AS_SEC;

typedef pte_t mem_flags_t;

static inline bool vm_mem_region_is_phys(bool reg) {return reg;}
static inline paddr_t vm_mem_region_get_phys(paddr_t phys, paddr_t base) 
{
    return phys;
}

void as_init(struct addr_space* as, enum AS_TYPE type, asid_t id, 
            pte_t* root_pt, colormap_t colors);
vaddr_t mem_alloc_vpage(struct addr_space* as, enum AS_SEC section,
                    vaddr_t at, size_t n);

#endif /* __MEM_PROT_H__ */
