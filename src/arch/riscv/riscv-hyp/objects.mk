## SPDX-License-Identifier: Apache-2.0
## Copyright (c) Bao Project and Contributors. All rights reserved.

cpu-objs-y+=$(ARCH_PROFILE)/boot.o
cpu-objs-y+=$(ARCH_PROFILE)/exceptions.o
cpu-objs-y+=$(ARCH_PROFILE)/root_pt.o
cpu-objs-y+=$(ARCH_PROFILE)/sbi.o
cpu-objs-y+=$(ARCH_PROFILE)/page_table.o
cpu-objs-y+=$(ARCH_PROFILE)/mem.o
cpu-objs-y+=$(ARCH_PROFILE)/vm.o
cpu-objs-y+=$(ARCH_PROFILE)/vmm.o
cpu-objs-y+=$(ARCH_PROFILE)/interrupts.o
cpu-objs-y+=$(ARCH_PROFILE)/sync_exceptions.o
cpu-objs-y+=$(ARCH_PROFILE)/cpu.o
