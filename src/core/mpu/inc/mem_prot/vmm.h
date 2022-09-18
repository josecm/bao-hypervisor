/**
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef MEM_PROT_VMM_H
#define MEM_PROT_VMM_H

struct vm_install_info {
    vaddr_t base_addr;
    size_t size;
    mem_flags_t flags;
};

#endif /* MEM_PROT_VMM_H */
