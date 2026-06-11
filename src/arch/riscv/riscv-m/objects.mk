## SPDX-License-Identifier: Apache-2.0
## Copyright (c) Bao Project and Contributors. All rights reserved.

# WORK IN PROGRESS - M-mode/PMP profile objects are added incrementally as the
# backend is implemented:
#   cpu-objs-y+=$(ARCH_PROFILE)/boot.o            # M-mode boot + OpenSBI handoff
#   cpu-objs-y+=$(ARCH_PROFILE)/exceptions.o      # mtvec trap entry/exit
#   cpu-objs-y+=$(ARCH_PROFILE)/sync_exceptions.o # mcause dispatch
#   cpu-objs-y+=$(ARCH_PROFILE)/interrupts.o      # mie/mip handling
#   cpu-objs-y+=$(ARCH_PROFILE)/pmp.o             # core/mpu PMP backend driver
#   cpu-objs-y+=$(ARCH_PROFILE)/mem.o             # as_arch_init (PMP)
#   cpu-objs-y+=$(ARCH_PROFILE)/vm.o              # S-mode vcpu setup
#   cpu-objs-y+=$(ARCH_PROFILE)/vmm.o             # medeleg/mideleg setup
#   cpu-objs-y+=$(ARCH_PROFILE)/sbi_guest.o       # guest S-mode SBI server
#   cpu-objs-y+=$(ARCH_PROFILE)/sbi_backend.o     # resident OpenSBI client
#   cpu-objs-y+=$(ARCH_PROFILE)/cpu.o             # cpu_arch_profile_init
