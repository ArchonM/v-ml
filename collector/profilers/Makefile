XLEN ?= 64

ifeq ($(XLEN),32)
ABI ?= ilp32d
endif
ifeq ($(XLEN),64)
ABI ?= lp64d
endif

src_dir := ..
profiler_dir := .
common_dir := $(src_dir)/benchmarks/common

RISCV_PREFIX ?= riscv$(XLEN)-unknown-elf-
RISCV_GCC ?= $(RISCV_PREFIX)gcc
RISCV_GCC_OPTS ?= -U_FORTIFY_SOURCE -DPREALLOCATE=1 -mcmodel=medany -static -std=gnu99 -O2 -ffast-math -fno-common -fno-builtin-printf -fno-tree-loop-distribute-patterns -Wno-implicit-int -Wno-implicit-function-declaration -mabi=$(ABI)
RISCV_LINK_OPTS ?= -static -nostdlib -nostartfiles -lm -lgcc -T $(common_dir)/test.ld
RISCV_OBJDUMP ?= $(RISCV_PREFIX)objdump --disassemble-all --disassemble-zeroes --section=.text --section=.text.startup --section=.text.init --section=.data
RISCV_MARCH ?= rv$(XLEN)gc
RISCV_SIM ?= spike --isa=$(RISCV_MARCH)
HPM_PROFILER_BUFFER_SIZE ?= 500
HPM_PROFILER_INTERVAL ?= 100

benchmarks := median multiply vvadd memcpy qsort rsort spmv towers

default: all

incs := -I$(profiler_dir) -I$(src_dir) -I$(common_dir) \
	-I$(src_dir)/benchmarks/median \
	-I$(src_dir)/benchmarks/multiply \
	-I$(src_dir)/benchmarks/vvadd \
	-I$(src_dir)/benchmarks/memcpy \
	-I$(src_dir)/benchmarks/qsort \
	-I$(src_dir)/benchmarks/rsort \
	-I$(src_dir)/benchmarks/spmv \
	-I$(src_dir)/benchmarks/towers

common_srcs := hpm_profiler.c $(common_dir)/syscalls.c $(common_dir)/crt.S
profiler_defs := -DHPM_PROFILER_BUFFER_SIZE=$(HPM_PROFILER_BUFFER_SIZE) -DHPM_PROFILER_INTERVAL=$(HPM_PROFILER_INTERVAL)

median_srcs := median_profiler.c $(src_dir)/benchmarks/median/median.c
multiply_srcs := multiply_profiler.c $(src_dir)/benchmarks/multiply/multiply.c
vvadd_srcs := vvadd_profiler.c
memcpy_srcs := memcpy_profiler.c
qsort_srcs := qsort_profiler.c
rsort_srcs := rsort_profiler.c
spmv_srcs := spmv_profiler.c
towers_srcs := towers_profiler.c

define compile_template
$(1).riscv: $$($(1)_srcs) $$(common_srcs) hpm_profiler.h $(src_dir)/HPC.h $(src_dir)/encoding.h
	$$(RISCV_GCC) $$(incs) $$(profiler_defs) $$(RISCV_GCC_OPTS) -march=$$(RISCV_MARCH) -o $$@ $$($(1)_srcs) $$(common_srcs) $$(RISCV_LINK_OPTS)
endef

$(foreach bmark,$(benchmarks),$(eval $(call compile_template,$(bmark))))

bins := $(addsuffix .riscv,$(benchmarks))
dumps := $(addsuffix .riscv.dump,$(benchmarks))
outs := $(addsuffix .riscv.out,$(benchmarks))

all: $(bins)
riscv: $(dumps)
run: $(outs)

%.riscv.dump: %.riscv
	$(RISCV_OBJDUMP) $< > $@

%.riscv.out: %.riscv
	$(RISCV_SIM) $< > $@

clean:
	rm -f $(bins) $(dumps) $(outs)

.PHONY: all riscv run clean
