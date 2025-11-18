## SPDX-License-Identifier: Apache-2.0
## Copyright (c) Bao Project and Contributors. All rights reserved.

CROSS_COMPILE ?= arm-none-eabi-

arch-cppflags+=-DAARCH32
arch-cflags+= -mfpu=vfpv3 -mfloat-abi=softfp
arch-asflags+=

## We need to use lgcc to use some of the 64-bit multiply/division helpers
## So we need to point out the linker the path for the libraries
lib_path := $(dir $(realpath $(shell $(cc) -print-libgcc-file-name)))
arch-ldflags+= -L$(lib_path) -lgcc 

clang_arch_target:=arm
