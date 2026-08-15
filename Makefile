obj-m := mhsrc.o

mhsrc-objs := SRC/main.o \
	LIB/mh_core.o LIB/mh_rule.o LIB/mh_reg.o LIB/mh_scan.o \
	LIB/mh_umount.o LIB/mh_ver.o \
	deps/KallRecon/lib/core.o deps/KallRecon/lib/slide.o \
	deps/KallRecon/lib/anchor.o \
	deps/HooKern/lib/hk.o deps/HooKern/lib/hk_ksym.o \
	deps/HooKern/lib/hk_patch.o deps/HooKern/lib/hk_ptr.o \
	deps/HooKern/lib/hk_kprobe.o deps/HooKern/lib/hk_kretprobe.o \
	deps/Type_info/lib/port.o deps/Type_info/lib/btf.o \
	deps/Type_info/lib/query.o deps/Type_info/lib/reg.o \
	deps/Type_info/lib/lib.o deps/Type_info/lib/anchor.o \
	deps/Type_info/lib/dwarf.o

ccflags-y += -std=gnu11
ccflags-y += -Wno-declaration-after-statement
ccflags-y += -Wno-unused-variable
ccflags-y += -Wno-unused-function
ccflags-y += -Wno-strict-prototypes
ccflags-y += -I$(src)/LIB
ccflags-y += -I$(src)/deps/KallRecon/lib
ccflags-y += -I$(src)/deps/HooKern/lib
ccflags-y += -I$(src)/deps/Type_info/lib

ifneq ($(TI_REMAP),0)
ccflags-y += -DCONFIG_TI_REMAP
endif

KDIR := $(KDIR)
MDIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
ODIR := $(MDIR)/out/$(VER)

$(info -- KDIR: $(KDIR))
$(info -- MDIR: $(MDIR))
$(info -- ODIR: $(ODIR))

all:
	make -C $(KDIR) M=$(ODIR) src=$(MDIR) modules
clean:
	make -C $(KDIR) M=$(ODIR) src=$(MDIR) clean

$(obj)/%.o: $(src)/%.c $(recordmcount_source) FORCE
	$(call if_changed_rule,cc_o_c)
	$(call cmd,force_checksrc)

$(obj)/%.o: $(src)/%.S FORCE
	$(call if_changed_rule,as_o_S)
