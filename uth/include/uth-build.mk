
output        ?=
csources      ?=
cxxsources    ?=
asmsources    ?=
build_dir     ?= .

uth_prefix    ?= 
uth_compiling ?= 0

libuth        ?= $(uth_prefix)/lib/libuth.a

osname := $(shell uname -s)

ifneq ($(OPTIMIZE),0)
    opts := -gdwarf-3 -O3
else
    opts := -gdwarf-3 -O0 -fno-inline
endif

armci_libdir := $(ARMCI_PREFIX)/lib
ifeq ($(shell ls $(armci_libdir) 2> /dev/null),)
    armci_libdir := $(ARMCI_PREFIX)/lib64
endif

libs     := -larmci
libdirs  := -L$(armci_libdir)
cxxstd   := -std=c++0x
defines  := -DMADI_DEBUG_LEVEL=$(DEBUG_LEVEL) #-DMADI_COMM_$(COMM_LAYER) -DMADI_OS_$(osname)
includes := -I$(uth_prefix)/include -I$(ARMCI_PREFIX)/include

ccwarnings := \
    -Wall \
    -Wextra \
    -Wformat=2 \
    -Winit-self \
    -Wmissing-include-dirs \
    -Wunused \
    -Wuninitialized \
    -Wfloat-equal \
    -Wpointer-arith \
    -Wwrite-strings \
    -Wconversion \
    -Wpacked \
    -Wstack-protector \
    -Woverlength-strings \
    -Wno-unused-variable \
    -Wno-unused-parameter \
    -Wno-unused-function \
    -Wno-unused-value

#    -Wstrict-overflow=3

cwarnings := \
    $(ccwarnings) \
    -Wstrict-prototypes \
    -Wold-style-definition \
    -Wmissing-prototypes \
    -Wnested-externs \
    -Wno-declaration-after-statement \
    -Wno-strict-overflow \
    -Wno-sign-conversion

cxxwarnings := \
    $(ccwarnings) \

#    -Weffc++

ifeq ($(uth_compiling),1)
    uth_cppflags := $(defines) $(includes) $(CPPFLAGS)
    uth_cflags   := $(opts) $(defines) $(includes) $(cwarnings) $(CFLAGS)
    uth_cxxflags := $(cxxstd) $(opts) $(defines) $(includes) $(cxxwarnings) \
                    $(CXXFLAGS)
    uth_ldflags  := $(libdirs) $(LDFLAGS)
    uth_libs     := $(libs)
else
    uth_cppflags := -I$(uth_prefix)/include \
	            $(defines) $(CPPFLAGS)
    uth_cflags   := -I$(uth_prefix)/include \
                    $(opts) $(defines) $(cwarnings) $(CFLAGS)
    uth_cxxflags := -I$(uth_prefix)/include \
	            $(cxxstd) $(opts) $(defines) $(cxxwarnings) $(CXXFLAGS)
    uth_ldflags  := -L$(uth_prefix)/lib $(libdirs) $(LDFLAGS)
    uth_libs     := -luth $(libs)
endif

show_flags   :=$(opts) $(defines) ...

cobjects   := $(csources:%.c=$(build_dir)/%.o)
cxxobjects := $(cxxsources:%.cc=$(build_dir)/%.o)
cxxobjects := $(cxxobjects:%.cpp=$(build_dir)/%.o)
asmobjects := $(asmsources:%.s=$(build_dir)/%.o)
asmobjects := $(asmobjects:%.S=$(build_dir)/%.o)

sources    := $(csources) $(cxxsources)
objects    := $(cobjects) $(cxxobjects) $(asmobjects)

depend    := $(build_dir)/depend


.PHONY: build clean distclean

build: $(depend) $(output)

$(output): $(objects) $(libuth)
	@printf "  %-3s ... $^\n" LD
	@$(LD) $(uth_ldflags) -o $(output) $(objects) $(uth_libs)

$(build_dir)/%.o: %.c
	@mkdir -p $(dir $@)
	@printf "  %-3s $(show_flags) $<\n" CC
	@$(CC) -c -o $@ $(uth_cflags) $<

$(build_dir)/%.o: %.cc
	@mkdir -p $(dir $@)
	@printf "  %-3s $(show_flags) $<\n" CXX
	@$(CXX) -c -o $@ $(uth_cxxflags) $<

$(build_dir)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@printf "  %-3s $(show_flags) $<\n" CXX
	@$(CXX) -c -o $@ $(uth_cxxflags) $<

$(build_dir)/%.o: %.S
	@mkdir -p $(dir $@)
	@printf "  %-3s $(show_flags) $<\n" CC
	@$(CC) -c -o $@ $(uth_cflags) $<

clean:
	-@rm -rf $(output) $(objects)

distclean:
	-@rm -rf $(output) $(build_dir)

# FIXME: add asmsources
$(depend): $(sources)
	-@ mkdir -p $(dir $@)
	-@ rm -f $@
	-@ touch $@
	-@ for i in $^; do \
            $(CXX) -E $(cxxstd) $(uth_cppflags) -MM $$i \
                | sed "s/\ [_a-zA-Z0-9][_a-zA-Z0-9\/]*\.cc\?//g" \
                | sed "s/\ [_a-zA-Z0-9][_a-zA-Z0-9\/]*\.cpp\?//g" \
                >> $@; \
        done
	-@ sed -e "s/\([_a-zA-Z0-9][_a-zA-Z0-9\/]*\)\.o/\.build\/\1\.o/" \
                $@ \
                > $(depend).tmp
	-@ sed -e "s/^\(.*[^\\]\)$$/\1 Makefile/g" \
                $(depend).tmp \
                > $@
	-@ rm -f $(depend).tmp

-include $(depend)

