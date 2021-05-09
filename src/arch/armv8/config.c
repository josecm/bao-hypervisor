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

#include <config.h>

void config_arch_vm_adjust_to_va(vm_config_t *vm_config, struct config *config, uint64_t phys)
{
    for (int i = 0; i < config->vmlist_size; i++) {
	    adjust_ptr(vm_config->platform.arch.smmu.smmu_groups, config);
    }
}
