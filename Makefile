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
MISRA_C2012_GUIDELINES:=

export root_dir:=$(abspath .)
docker_dir:=$(root_dir)/docker

ifneq ($(BAO_DOCKER_ENABLE),)

all .DEFAULT:
	@$(MAKE) -C $(docker_dir) $@

.PHONY: all

else #BAO_DOCKER_ENABLE

all DEFAULT:
	@$(MAKE) -f build.mk $(MAKECMDGOALS)

format_srcs:=$(shell find $(root_dir)/src -regex ".*\.\(c\|h\)")
format:
	@echo "Running $(clang-format)..."
	@$(clang-format) --style=file -i $(format_srcs)
format-check:
	@diff <(cat $(format_srcs)) <($(clang-format) --style=file $(format_srcs))

ifneq ($(findstring $(MAKECMDGOALS), tidy, cppcheck, misra-check),)
include setup.mk
endif

tidy:
	@$(clang-tidy) $(c_srcs) $(h_srcs) -- --target=$(clang-arch) $(CPPFLAGS)

cppcheck_flags:= --quiet --enable=all --error-exitcode=1 $(CPPFLAGS)
std_incs:=$(shell $(CROSS_COMPILE)gcc -E -Wp,-v -xc /dev/null 2>&1 | grep "^ ")

cppcheck:
	@$(CPPCHECK) $(cppcheck_flags) $(addprefix -I , $(std_incs)) $(c_srcs)

misra_dir:=$(abspath misra)
misra_rules:=$(misra_dir)/rules.txt
cppcheck_misra_addon:=$(misra_dir)/misra.json
cppcheck_misra_flags:= --quiet --suppress=all --error-exitcode=1 --addon=$(cppcheck_misra_addon) $(CPPFLAGS)
zephyr_coding_guidelines:=https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/doc/contribute/coding_guidelines/index.rst

ifeq ($(MISRA_C2012_GUIDELINES),)
$(misra_rules):
	@echo "Appendix A Summary of guidelines" > $@
	-@wget -q -O - $(zephyr_coding_guidelines) | grep "\* -  Rule" -A 2 | sed -n '2~2!s/\(.\{9\}\)//p' >> $@
else
$(misra_rules):
	@pdftotext $(MISRA_C2012_GUIDELINES) $@
endif

misra-check: $(misra_rules)
	@$(CPPCHECK) $(cppcheck_misra_flags) $(c_srcs) $(h_srcs)

misra-clean:
	-rm -f $(misra_rules)
	-find . -name "*.dump" | xargs rm -f

clean: misra-clean
	-@$(MAKE) -f build.mk $(MAKECMDGOALS)

endif #BAO_DOCKER_ENABLE
