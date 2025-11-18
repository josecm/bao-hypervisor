/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <mem.h>

void as_arch_init(struct addr_space* as)
{
    mpu_struct_init(&as->arch.mpu);
}
