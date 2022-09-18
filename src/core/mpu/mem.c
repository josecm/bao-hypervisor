/**
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <mem.h>
#include <cpu.h>
#include <bao.h>
#include <fences.h>
#include <platform_defs.h>

extern bool mem_reserve_ppages(struct ppages *ppages);
extern bool mem_are_ppages_reserved(struct ppages *ppages);
extern void mem_write_mp(paddr_t pa, size_t n, mem_flags_t flags);
extern void mem_free_physical_region(size_t num_region);
extern unsigned long mem_get_granularity();
void mem_msg_handler(uint32_t event, uint64_t data);
extern void cpu_empty_mailbox();
extern void mem_read_physical_entry(struct memory_protection *mp_entry, 
                                    unsigned long region);
extern struct shared_region* cpu_msg_get_mem_alloc();

static inline mem_flags_t mem_prot_broadcast(mem_flags_t flags)
{
    return (flags & MEM_PROT_FLAG_SH_MASK);
}

void update_virtual_memprot(struct addr_space *as, vaddr_t va, size_t n, 
                            mem_flags_t flags, unsigned long reg)
{
    cpu()->as.mem_prot[reg].base_addr = va;
    cpu()->as.mem_prot[reg].limit_addr = n;
    cpu()->as.mem_prot[reg].mem_flags = flags;
    cpu()->as.mem_prot[reg].assigned = true;
    bitmap_set(cpu()->arch.profile.mem_p, reg);
}

void mem_prot_boot_sync()
{
    struct memory_protection mp_entry;
    unsigned long region_num = 0;
    mem_read_physical_entry(&mp_entry, region_num);
    while(mp_entry.assigned)
    {
        update_virtual_memprot(&cpu()->as, mp_entry.base_addr, 
                              (mp_entry.limit_addr - mp_entry.base_addr), 
                              mp_entry.mem_flags, region_num);
        region_num++;
        mem_read_physical_entry(&mp_entry, region_num);
    }
}

void mem_prot_init() {
    as_init(&cpu()->as, AS_HYP, 0);
    cpu_broadcast_init(&cpu()->as);
    cpu_sync_barrier(&cpu_glb_sync);    
    cpu_mem_prot_bitmap_init(&cpu()->arch.profile);
    list_init(&cpu()->interface->memprot.shared_mem_prot);
    mem_prot_boot_sync();
}

size_t mem_cpu_boot_alloc_size() {
    size_t size = ALIGN(sizeof(struct cpu), PAGE_SIZE);
    return size;
}

void as_init(struct addr_space *as, enum AS_TYPE type, 
            colormap_t colors)
{
    as->type = type;
    as->colors = 0;
    as->cpus = 0;
    as_arch_init(as);
    for(size_t i=0; i<MPU_ABST_ENTRIES; i++)
    {
        as->mem_prot[i].assigned = false;
        as->mem_prot[i].base_addr = 0;
        as->mem_prot[i].limit_addr = 0;
        as->mem_prot[i].mem_flags = 0;
    }
}

mpid_t mem_get_available_region(struct addr_space *as)
{   
    for (mpid_t i=0; i<MPU_ABST_ENTRIES; i++)
        if (!as->mem_prot[i].assigned) return i;
    
    return -1;
}

void mem_set_shared_region(vaddr_t va, size_t n, mem_flags_t flags)
{
    if (n < mem_get_granularity())
            ERROR ("region must be bigger than granularity");

    mpid_t region_num = mem_get_available_region(&cpu()->as);
    if(region_num>=0) {
        cpu()->as.mem_prot[region_num].assigned = true;
        cpu()->as.mem_prot[region_num].base_addr = va;
        cpu()->as.mem_prot[region_num].limit_addr = (va+n);
        cpu()->as.mem_prot[region_num].mem_flags = flags;
        mem_write_mp(va, n, flags);
    }

}

void mem_write_broadcast_region(uint64_t data)
{
    bool erase_message = false;
    struct list* temp_list = NULL;
    struct shared_region* region = NULL;
    unsigned long base_addr = 0;
    unsigned long size = 0;
    unsigned long mem_flags = 0;

    *temp_list = cpu_interfaces[data].memprot.shared_mem_prot;
    region = (struct shared_region*) temp_list->head;
    
    size_t unread_message = bitmap_get(&region->trgt_cpu, cpu()->id);
 
        /*  Walk the list till find an unread message */
    while (temp_list->head != NULL && unread_message == 0){
        unread_message = bitmap_get(&region->trgt_cpu, cpu()->id);
        if (unread_message == 0)
        {
            temp_list->head = *temp_list->head;
            region = (struct shared_region*) temp_list->head;
        }
    }

    if(unread_message)
    {
        base_addr = region->base_addr;
        size = region->size;
        mem_flags = region->mem_flags;
        /*  Clear itself from the unread message list   */
        spin_lock(&region->trgt_bitmap_lock);
    
        bitmap_clear(&region->trgt_cpu, cpu()->id);
        /*  Check if the message node needs to be popped */
        size_t unread = bitmap_count(&region->trgt_cpu, 0, PLAT_CPU_NUM, 1);
        if (unread == 0)
        {
            erase_message = true;
        }

        spin_unlock(&region->trgt_bitmap_lock);
    
        if (erase_message)
        {
            if (temp_list->head == temp_list->tail)
            { 
                list_pop(&cpu_interfaces[data].memprot.shared_mem_prot);
            }
            else list_rm(&cpu_interfaces[data].memprot.shared_mem_prot, temp_list->head);
        }

        mem_set_shared_region(base_addr, size, mem_flags);
    }
}

CPU_MSG_HANDLER(mem_msg_handler, MEM_PROT_SYNC);

void mem_msg_handler(uint32_t event, uint64_t data)
{
    switch(event){
        case MP_MSG_REGION:
            mem_write_broadcast_region(data);
        break;
    }
}

void mem_region_broadcast(struct addr_space *as, vaddr_t va, size_t n, 
                            mem_flags_t flags)
{
    struct shared_region *node = cpu_msg_get_mem_alloc();
    node->base_addr = va;
    node->size = n;
    node->mem_flags = flags;
    node->trgt_bitmap_lock = SPINLOCK_INITVAL;
    node->trgt_cpu = as->cpus;

        /* Cleaning itself from the unread list */
    unsigned long sender_index = cpu()->id;
    bitmap_clear(&node->trgt_cpu,sender_index);


    list_push(&cpu()->interface->memprot.shared_mem_prot, (node_t*) node);

    struct cpu_msg msg = {MEM_PROT_SYNC, MP_MSG_REGION, cpu()->id};
    
    for (size_t i = 0; i < PLAT_CPU_NUM; i++) {
        if (bitmap_get(&node->trgt_cpu, i)) {
            cpu_send_msg(i, &msg);
        }
    }
}

unsigned long mem_section_shareable(enum AS_SEC section)
{
    unsigned long broadcast = 0;
    
    if (section == SEC_HYP_GLOBAL || section == SEC_VM_ANY)  broadcast = 1;
    
    return broadcast;
}

void mem_set_region(struct addr_space *as, vaddr_t va, size_t n, mem_flags_t flags)
{
    if (n < mem_get_granularity())
            ERROR ("region must be bigger than granularity");

    mpid_t region_num = mem_get_available_region(as);
    if(region_num>=0) {
        as->mem_prot[region_num].assigned = true;
        as->mem_prot[region_num].base_addr = va;
        as->mem_prot[region_num].limit_addr = (va+n);
        as->mem_prot[region_num].mem_flags = flags;
        mem_write_mp(va, n, flags);
    }

    if(mem_prot_broadcast(flags)){
        mem_region_broadcast(as, va, n, flags);
    }
}

mpid_t mem_get_address_region(struct addr_space* as, paddr_t addr)
{
    for(size_t i=0; i<MPU_ABST_ENTRIES && as->mem_prot[i].assigned; i++)
        if((addr >= as->mem_prot[i].base_addr) && (
            addr <= as->mem_prot[i].limit_addr)) return i;

    return -1;
}

bool mem_free_region_by_address(struct addr_space* as, paddr_t addr)
{
    mpid_t reg_num = mem_get_address_region(as, addr);
    if (reg_num >= 0) mem_free_physical_region(addr);
        else return false;
    return true;
}

bool mem_erase_region_by_address(struct addr_space* as, paddr_t addr)
{
    mpid_t reg_num = mem_get_address_region(as, addr);
    if (reg_num >= 0){
        as->mem_prot[reg_num].assigned = false;
        as->mem_prot[reg_num].base_addr = 0;
        as->mem_prot[reg_num].limit_addr = 0;
        as->mem_prot[reg_num].mem_flags = 0;
        mem_free_physical_region(addr);
    } 
        else return false;
    return true;
}

void merge_regions(struct addr_space* as, struct memory_protection* dst, 
                    struct memory_protection* origin)
{
    dst->limit_addr = origin->limit_addr;
    mem_erase_region_by_address(as, origin->base_addr);
}

void search_regions_to_merge(struct addr_space* as)
{
    for(size_t i=0; i<(MPU_ABST_ENTRIES-1) && as->mem_prot[i].assigned; i++)
    {
        for (size_t j=(i+1); j<(MPU_ABST_ENTRIES-1) && as->mem_prot[j].assigned; j++)
            if((as->mem_prot[i].limit_addr) ==
                as->mem_prot[j].base_addr)
                {
                    if(as->mem_prot[i].mem_flags ==
                        as->mem_prot[j].mem_flags)
                    {
                        merge_regions(as, &as->mem_prot[i],
                                        &as->mem_prot[j]);
                    }
                }
    }
}

bool mem_map(struct addr_space *as, vaddr_t va, struct ppages *ppages,
            size_t n, mem_flags_t flags)
{
        /* Address does not belong to a device/shared memory region */
        /* TODO: Redefine PTE_HYP_DEV_FLAGS value */ 
    if (va != ppages->base) {
        ERROR ("Trying to map non identity");
    }

    if (ppages == NULL && !mem_are_ppages_reserved(ppages)){
        ERROR ("Error mapping memory! Memory was not allocated");
    }

    mem_set_region(as, va, (n * PAGE_SIZE), flags);

    return true;
}

vaddr_t mem_alloc_map(struct addr_space* as, enum AS_SEC section, struct ppages *page,
                        vaddr_t at, size_t size, mem_flags_t flags)
{
    // TODO: Check if page->base, page->size and vaddr_t at are page_size align?
    vaddr_t address = NULL_VA;
    if (at == address) {
        at = page->base;
    }
    else if (page == NULL) {page->base = at;}
    else if (at != page->base) {
        ERROR ("Address must match its page base address");
    }
    
    unsigned long shareability = mem_section_shareable(section);
    flags |= (shareability << MEM_PROT_FLAG_SH_OFFSET);
 
    if (page == NULL){
        struct ppages temp_page = mem_ppages_get(at, size);
        mem_map(as, at, &temp_page, size, flags);
    }
    else {
        mem_map(as, at, page, size, flags);
    }

    return at;
}

vaddr_t mem_alloc_map_dev(struct addr_space* as, enum AS_SEC section,
                             vaddr_t at, paddr_t pa, size_t size)
{
    struct ppages temp_page = mem_ppages_get(pa, size);
    return mem_alloc_map(as, section, &temp_page, at, size,
                   as->type == AS_HYP ? PTE_HYP_DEV_FLAGS : PTE_VM_DEV_FLAGS);
}

vaddr_t mem_map_cpy(struct addr_space *ass, struct addr_space *asd, vaddr_t vas,
                vaddr_t vad, size_t n) {

    return vas;
}

void mem_unmap(struct addr_space *as, vaddr_t at, size_t n,
                    bool free_ppages)
{
    mem_erase_region_by_address(as, at);
}
