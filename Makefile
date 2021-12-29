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
MAKEFLAGS+= --no-print-directory
CLANG_VERSION:=12
clang-format:=clang-format-$(CLANG_VERSION)
clang-tidy:=clang-tidy-$(CLANG_VERSION)
CPPCHECK:=cppcheck

export root_dir:=$(abspath .)
docker_dir:=$(root_dir)/docker

ifneq ($(BAO_DOCKER_ENABLE),)

all .DEFAULT:
	@$(MAKE) -C $(docker_dir) $@

.PHONY: all

else #BAO_DOCKER_ENABLE

all .DEFAULT:
	@$(MAKE) -f build.mk $@

format_srcs:=$(shell find $(root_dir)/src -regex ".*\.\(c\|h\)")
format:
	@echo "Running $(clang-format)..."
	@$(clang-format) --style=file -i $(format_srcs)
format-check:
	@diff <(cat $(format_srcs)) <($(clang-format) --style=file $(format_srcs))

ifneq ($(findstring $(MAKECMDGOALS), tidy, cppcheck),)
include setup.mk
endif

tidy:
	@$(clang-tidy) $(c_srcs) $(h_srcs) -- --target=$(clang-arch) $(CPPFLAGS)

cppcheck_flags:= --quiet --enable=all --error-exitcode=1 $(CPPFLAGS)
std_incs:=$(shell $(CROSS_COMPILE)gcc -E -Wp,-v -xc /dev/null 2>&1 | grep "^ ")

cppcheck:
	@$(CPPCHECK) $(cppcheck_flags) $(addprefix -I , $(std_incs)) $(c_srcs)

endif #BAO_DOCKER_ENABLE
