/**
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef __ARCH_MEM_H__
#define __ARCH_MEM_H__

#include <bao.h>

#define PTE_INVALID (0)
#define PTE_HYP_FLAGS (0x38)
#define PTE_HYP_DEV_FLAGS (0x40)

#define PTE_VM_FLAGS (0x38)
#define PTE_VM_IMG_FLAGS (0x3A)
#define PTE_VM_DEV_FLAGS PTE_VM_IMG_FLAGS

#endif /* __ARCH_MEM_H__ */
