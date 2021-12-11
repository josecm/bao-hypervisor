## 
 # Bao, a Lightweight Static Partitioning Hypervisor 
 #
 # Copyright (c) Bao Project (www.bao-project.org), 2019-
 #
 # Authors:
 #      Jose Martins <jose.martins@bao-project.org>
 #
 # Bao is free software; you can redistribute it and/or modify it under the
 # terms of the GNU General Public License version 2 as published by the Free
 # Software Foundation, with a special exception exempting guest code from such
 # license. See the COPYING file in the top-level directory for details. 
 #
##

SHELL:=bash

# Setup toolchain macros
cpp=		$(CROSS_COMPILE)cpp
sstrip= 	$(CROSS_COMPILE)strip
cc=			$(CROSS_COMPILE)gcc
ld = 		$(CROSS_COMPILE)ld
as=			$(CROSS_COMPILE)as
objcopy=	$(CROSS_COMPILE)objcopy
objdump=	$(CROSS_COMPILE)objdump
readelf=	$(CROSS_COMPILE)readelf
size=		$(CROSS_COMPILE)size

#Makefile arguments and default values
DEBUG:=y
OPTIMIZATIONS:=2
CONFIG_BUILTIN=n
CONFIG=
PLATFORM=

# Directories
cur_dir:=$(abspath .)
src_dir:=$(cur_dir)/src
cpu_arch_dir=$(src_dir)/arch
lib_dir=$(src_dir)/lib
core_dir=$(src_dir)/core
platforms_dir=$(src_dir)/platform
configs_dir=$(cur_dir)/configs

#Plataform must be defined excpet for clean target
ifeq ($(PLATFORM),) 
ifneq ($(MAKECMDGOALS), clean)
 $(error Target platform argument (PLATFORM) not specified)
endif
endif

platform_dir=$(platforms_dir)/$(PLATFORM)
drivers_dir=$(platforms_dir)/drivers

ifeq ($(wildcard $(platform_dir)),)
 $(error Target platform $(PLATFORM) is not supported)
endif

-include $(platform_dir)/platform.mk	# must define ARCH and CPU variables
cpu_arch_dir=$(src_dir)/arch/$(ARCH)
cpu_impl_dir=$(cpu_arch_dir)/impl/$(CPU)
-include $(cpu_arch_dir)/arch.mk

src_dirs:= $(cpu_arch_dir) $(cpu_impl_dir) $(lib_dir) $(core_dir)\
	$(platform_dir) $(addprefix $(drivers_dir)/, $(drivers))
inc_dirs:=$(addsuffix /inc, $(src_dirs))

# Setup list of objects for compilation
-include $(addsuffix /objects.mk, $(src_dirs))

objs-y:=
objs-y+=$(addprefix $(cpu_arch_dir)/, $(cpu-objs-y))
objs-y+=$(addprefix $(lib_dir)/, $(lib-objs-y))
objs-y+=$(addprefix $(core_dir)/, $(core-objs-y))
objs-y+=$(addprefix $(platform_dir)/, $(boards-objs-y))
objs-y+=$(addprefix $(drivers_dir)/, $(drivers-objs-y))

c_srcs:=$(wildcard $(objs-y:%.o=%.c))
h_srcs:=$(foreach dir, $(inc_dirs), $(wildcard $(dir)/*.h))

# Toolchain flags
override CPPFLAGS+=$(addprefix -I, $(inc_dirs)) $(arch-cppflags) $(platform-cppflags)
vpath:.=CPPFLAGS

ifeq ($(DEBUG), y)
	debug_flags:=-g
endif

override CFLAGS+=-O$(OPTIMIZATIONS) -Wall -Werror -ffreestanding -std=gnu11 \
	 -mstrict-align -fno-pic $(arch-cflags) $(platform-cflags) $(CPPFLAGS) \
	 $(debug_flags)

override ASFLAGS+=$(CFLAGS) $(arch-asflags) $(platform-asflags)
override LDFLAGS+=-build-id=none -nostdlib $(arch-ldflags) $(plattform-ldflags)
