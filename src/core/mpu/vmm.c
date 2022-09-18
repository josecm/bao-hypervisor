/**
 * SPDX-License-Identifier: GPL-2.0 
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <vmm.h>
#include <io.h>

void vmm_io_init() {

}

struct vm_install_info vmm_get_vm_install_info(struct vm* vm, size_t ncpus) {
    return (struct vm_install_info){.base_addr = (vaddr_t)vm, 
                                    .size = sizeof(*vm) + 
                                            (ncpus * sizeof(struct vcpu)) };
}

void vmm_vm_install(struct vm* vm, struct vm_install_info *install_info) {
    mem_set_shared_region(install_info->base_addr, install_info->size,
                            PTE_VM_FLAGS);
}
