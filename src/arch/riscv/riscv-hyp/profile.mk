## SPDX-License-Identifier: Apache-2.0
## Copyright (c) Bao Project and Contributors. All rights reserved.

# HS-mode hypervisor profile: relies on the RISC-V hypervisor (H) extension
# and 2-stage MMU translation for guest isolation. Assumes an M-mode firmware
# (e.g. OpenSBI) is resident below Bao.

arch-cflags+=
arch-asflags+=
arch-ldflags+=

arch_mem_prot:=mmu
PAGE_SIZE:=0x1000
