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

PROJECT_NAME:=bao

include setup.mk

# List existing submakes
submakes:=config

build_dir:=$(cur_dir)/build/$(PLATFORM)
builtin_build_dir:=$(build_dir)/builtin-configs
bin_dir:=$(cur_dir)/bin/$(PLATFORM)
directories:=$(build_dir) $(bin_dir) $(builtin_build_dir) 

deps+=$(patsubst %.o,%.d,$(objs-y))
objs-y:=$(patsubst $(src_dir)%, $(build_dir)%, $(objs-y))

build_dirs:=$(patsubst $(src_dir)%, $(build_dir)%, $(src_dirs) $(inc_dirs))
directories+=$(build_dirs)

CONFIG_REPO?=$(configs_dir)
ifeq ($(CONFIG_BUILTIN), y)
ifeq ($(CONFIG),)
 $(error Buil-in configuration enabled but no configuration (CONFIG) specified)
endif
bin_dir:=$(bin_dir)/builtin-configs/$(CONFIG)
builtin-config-obj:=$(builtin_build_dir)/$(CONFIG).o
objs-y+=$(builtin-config-obj)
endif

# Setup list of targets for compilation
targets-y+=$(bin_dir)/$(PROJECT_NAME).elf
targets-y+=$(bin_dir)/$(PROJECT_NAME).bin

# Generated files variables

ld_script:= $(src_dir)/linker.ld
ld_script_temp:= $(build_dir)/linker_temp.ld
deps+=$(ld_script_temp).d

asm_defs_src:=$(cpu_arch_dir)/asm_defs.c
asm_defs_hdr:=$(patsubst $(src_dir)%, $(build_dir)%, \
	$(cpu_arch_dir))/inc/asm_defs.h
inc_dirs+=$(patsubst $(src_dir)%, $(build_dir)%, $(cpu_arch_dir))/inc
deps+=$(asm_defs_hdr).d

gens:=
gens+=$(asm_defs_hdr)

.PHONY: all
all: $(targets-y)
	
$(bin_dir)/$(PROJECT_NAME).elf: $(gens) $(objs-y) $(ld_script_temp)
	@echo "Linking			$(patsubst $(cur_dir)/%, %, $@)"
	@$(ld) $(LDFLAGS) -T$(ld_script_temp) $(objs-y) -o $@
	@$(objdump) -S --wide $@ > $(basename $@).asm
	@$(readelf) -a --wide $@ > $@.txt

ifneq ($(DEBUG), y)
	@echo "Striping	$@"
	@$(sstrip) -s $@
endif

$(ld_script_temp):
	@echo "Pre-processing		$(patsubst $(cur_dir)/%, %, $(ld_script))"
	@$(cc) -E $(addprefix -I, $(inc_dirs)) -x assembler-with-cpp $(ld_script) \
		| grep -v '^\#' > $(ld_script_temp)

ifeq (, $(findstring $(MAKECMDGOALS), clean $(submakes)))
-include $(deps)
endif

$(ld_script_temp).d: $(ld_script) 
	@echo "Creating dependecy	$(patsubst $(cur_dir)/%, %, $<)"
	@$(cc) -x assembler-with-cpp  -MM -MT "$(ld_script_temp) $@" \
		$(addprefix -I, $(inc_dirs))  $< > $@

$(build_dir)/%.d : $(src_dir)/%.[c,S] | $(gens)
	@echo "Creating dependecy	$(patsubst $(cur_dir)/%, %, $<)"
	@$(cc) -MM -MG -MT "$(patsubst %.d, %.o, $@) $@"  $(CPPFLAGS) $< > $@	

$(objs-y):
	@echo "Compiling source	$(patsubst $(cur_dir)/%, %, $<)"
	@$(cc) $(CFLAGS) -c $< -o $@

%.bin: %.elf
	@echo "Generating binary	$(patsubst $(cur_dir)/%, %, $@)"
	@$(objcopy) -S -O binary $< $@

#Generate assembly macro definitions from arch/$(ARCH)/$(asm_defs_src) if such
#	file exists

ifneq ($(wildcard $(asm_defs_src)),)
$(asm_defs_hdr): $(asm_defs_src)
	@echo "Generating header	$(patsubst $(cur_dir)/%, %, $@)"
	@$(cc) -S $(CFLAGS) $< -o - \
		| awk '($$1 == "->") { print "#define " $$2 " " $$3 }' > $@

$(asm_defs_hdr).d: $(asm_defs_src)
	@echo "Creating dependecy	$(patsubst $(cur_dir)/%, %,\
		 $(patsubst %.d,%, $@))"
	@$(cc) -MM -MT "$(patsubst %.d,%, $@)" $(addprefix -I, $(inc_dirs)) $< > $@	
endif

ifdef CONFIG
include $(configs_dir)/configs.mk
all: config
endif

ifeq ($(CONFIG_BUILTIN), y)
override CPPFLAGS+=-DCONFIG_BIN=$(CONFIG_BIN)
builtin-config-src:=$(core_dir)/builtin-config.S
$(builtin-config-obj): $(builtin-config-src) $(CONFIG_BIN)
endif

#Generate directories for object, dependency and generated files

.SECONDEXPANSION:

$(objs-y) $(deps) $(targets-y) $(gens): | $$(@D)

$(directories):
	@echo "Creating directory	$(patsubst $(cur_dir)/%, %, $@)"
	@mkdir -p $@

#Clean all object, dependency and generated files

.PHONY: clean
clean:
	@echo "Erasing directories..."
	-rm -rf $(build_dir)
	-rm -rf $(bin_dir)
