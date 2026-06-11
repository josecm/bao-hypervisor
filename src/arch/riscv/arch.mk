## SPDX-License-Identifier: Apache-2.0
## Copyright (c) Bao Project and Contributors. All rights reserved.

CROSS_COMPILE ?= riscv64-unknown-elf-

# Interrupt controller source files
ifeq ($(IRQC), PLIC)
IRQC_DIR?=plic
else ifeq ($(IRQC), APLIC)
IRQC_DIR?=aia
else ifeq ($(IRQC),)
$(error Platform must define IRQC)
else
$(error Invalid IRQC $(IRQC))
endif

irqc_arch_dir=$(cpu_arch_dir)/irqc/$(IRQC_DIR)
src_dirs+=$(irqc_arch_dir)

arch-cppflags+=-DIRQC=$(IRQC)
arch-cflags = -mcmodel=medany -march=rv64g -mstrict-align
arch-asflags =
arch-ldflags =

# Architecture profile selection. Each profile picks the privilege model and
# memory protection backend Bao runs with:
#   riscv-hyp - HS-mode hypervisor using the H-extension and 2-stage MMU
#               translation (default; requires an M-mode firmware below).
#   riscv-m   - M-mode static-partitioning monitor using PMP for isolation,
#               for cores without the H-extension.
ARCH_PROFILE?=riscv-hyp
arch_profile_dir:=$(cpu_arch_dir)/$(ARCH_PROFILE)
include $(arch_profile_dir)/profile.mk
src_dirs+=$(arch_profile_dir)
