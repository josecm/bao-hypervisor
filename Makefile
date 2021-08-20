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

CLANG_FORMAT:=clang-format-12

export root_dir:=$(abspath .)
docker_dir:=$(root_dir)/docker

c_srcs:=$(shell find $(root_dir)/src -regex ".*\.\(c\|h\)")

ifneq ($(BAO_DOCKER_ENABLE),)

all .DEFAULT:
	@$(MAKE) -C $(docker_dir) $@

.PHONY: all

else #BAO_DOCKER_ENABLE

all .DEFAULT:
	@$(MAKE) -f build.mk $@

format:
	@echo "Running $(CLANG_FORMAT)..."
	@$(CLANG_FORMAT) --style=file -i $(c_srcs)

format-check:
	@diff <(cat $(c_srcs)) <($(CLANG_FORMAT) --style=file $(c_srcs))

endif #BAO_DOCKER_ENABLE
