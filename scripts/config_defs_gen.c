/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved
 */

#include <stdio.h>
#include <config.h>
static size_t vm_num = 0;
static size_t vcpu_num = 0;

size_t vm_count_structs(struct vm_config *vm)
{
    vm_num += 1;
    vcpu_num += vm->platform.cpu_num;
    for (size_t i = 0; i < vm->children_num; i++) {
        vm_count_structs(vm->children[i]);
    }
}

int main() {

    size_t root_vcpu_num = 0;

    for (size_t i = 0; i < config.vmlist_size; i++) {
        root_vcpu_num += config.vmlist[i]->platform.cpu_num;
    }

    for (size_t i = 0; i < config.vmlist_size; i++) {
        vm_count_structs(config.vmlist[i]);
    }

    printf("#define CONFIG_VM_NUM %ld\n", vm_num);
    printf("#define CONFIG_VCPU_NUM %ld\n", vcpu_num);
    printf("#define CONFIG_ROOT_VCPU_NUM %ld\n", vcpu_num);

    if(config.hyp.relocate) {
        printf("#define CONFIG_HYP_BASE_ADDR (0x%lx)\n", config.hyp.base_addr);
    } else {
        printf("#define CONFIG_HYP_BASE_ADDR PLAT_BASE_ADDR\n");
    }

    return 0;
 }
