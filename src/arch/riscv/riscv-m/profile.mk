## SPDX-License-Identifier: Apache-2.0
## Copyright (c) Bao Project and Contributors. All rights reserved.

# M-mode static-partitioning profile: for RISC-V cores WITHOUT the hypervisor
# (H) extension. Bao runs in machine mode, owns mtvec, and isolates partitions
# using Physical Memory Protection (PMP) instead of 2-stage MMU translation.
# Guests run in S/U-mode and their S-mode ecalls trap to Bao's M-mode SBI
# server; generic platform SBI is relayed to a separately-built resident
# OpenSBI through a fixed handoff/call ABI (see README.md).
#
# WORK IN PROGRESS: the sources for this profile are being added incrementally.
# No platform selects it yet; selecting ARCH_PROFILE=riscv-m will not link until
# the M-mode boot, trap, PMP and SBI sources land.

arch-cflags+=
arch-asflags+=
arch-ldflags+=

# PMP-based isolation reuses the core MPU memory-protection backend.
arch_mem_prot:=mpu
# PMP NAPOT granularity is 4 bytes, but Bao's region bookkeeping uses a
# conventional page granule; partition regions are still naturally aligned.
PAGE_SIZE:=0x1000
