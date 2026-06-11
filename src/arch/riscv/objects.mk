## SPDX-License-Identifier: Apache-2.0
## Copyright (c) Bao Project and Contributors. All rights reserved.

# Profile-agnostic objects, shared by all RISC-V architecture profiles.
cpu-objs-y+=cache.o
cpu-objs-y+=iommu.o
cpu-objs-y+=relocate.o

# Profile-specific objects are added by $(ARCH_PROFILE)/objects.mk.
