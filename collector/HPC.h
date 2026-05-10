#ifndef HPC_H
#define HPC_H

#include <stdint.h>
#include <stdio.h>
#include "encoding.h"

//Events

#define EVENT_INSTR 0
#define EVENT_STALL 1
#define EVENT_MEM 2

//Instruction Macros

#define HPM_EXCEPTION (1 << 0)
#define HPM_LOAD (1 << 1)
#define HPM_STORE (1 << 2)
#define HPM_AMO (1 << 3)
#define HPM_SYS (1 << 4)
#define HPM_ARITH (1 << 5)
#define HPM_BRANCH (1 << 6)
#define HPM_JAL (1 << 7)
#define HPM_JALR (1 << 8)
#define HPM_MUL (1 << 9)
#define HPM_DIV (1 << 10)
#define HPM_LOAD_FP (1 << 11)
#define HPM_STORE_FP (1 << 12)
#define HPM_ADD_FP (1 << 13)
#define HPM_MUL_FP (1 << 14)
#define HPM_MADD_FP (1 << 15)
#define HPM_DIVSQRT_FP (1 << 16)
#define HPM_OTHER_FP (1 << 17)

//Stall Macros

#define LOAD_USE_INTLK (1 << 0)
#define LONG_INTLK (1 << 1)
#define CSR_INTLK (1 << 2)
#define ICACHE_BLK (1 << 3)
#define DCACHE_CLK (1 << 4)
#define BR_MISPRED (1 << 5)
#define TGT_MISPRED (1 << 6)
#define FLUSH (1 << 7)
#define REPLAY (1 << 8)
#define MUL_DIV_INTLK (1 << 9)
#define FP_INTLK (1 << 10)

//Memory Macros

#define ICACHE_MISS (1 << 0)
#define DCACHE_MISS (1 << 1)
#define DCACHE_REL (1 << 2)
#define ITLB_MISS (1 << 3)
#define DTLB_MISS (1 << 4)
#define L2TB_MISS (1 << 5)

//Event Encoding

#define HPM_ENCODE(set, mask) (((mask) << 8 | (set)))

//initialization


static inline void HPC_INIT_ALL() {
	write_csr(mcountinhibit, 0);
};

//Assign counters


static inline void HPC_ASSIGN(int counter, uint32_t set, uint32_t mask) {
	uint32_t val = HPM_ENCODE(set, mask);

	switch (counter) {
		case 3 : write_csr(mhpmevent3, val); break;
		case 4 : write_csr(mhpmevent4, val); break;
		case 5 : write_csr(mhpmevent5, val); break;
		case 6 : write_csr(mhpmevent6, val); break;
		case 7 : write_csr(mhpmevent7, val); break;
		case 8 : write_csr(mhpmevent8, val); break;
		case 9 : write_csr(mhpmevent9, val); break;
		case 10 : write_csr(mhpmevent10, val); break;
		case 11 : write_csr(mhpmevent11, val); break;
		case 12 : write_csr(mhpmevent12, val); break;
		case 13 : write_csr(mhpmevent13, val); break;
		case 14 : write_csr(mhpmevent14, val); break;
		case 15 : write_csr(mhpmevent15, val); break;
		case 16 : write_csr(mhpmevent16, val); break;
		case 17 : write_csr(mhpmevent17, val); break;
		case 18 : write_csr(mhpmevent18, val); break;
		case 19 : write_csr(mhpmevent19, val); break;
		case 20 : write_csr(mhpmevent20, val); break;
		case 21 : write_csr(mhpmevent21, val); break;
		case 22 : write_csr(mhpmevent22, val); break;
		case 23 : write_csr(mhpmevent23, val); break;
		case 24 : write_csr(mhpmevent24, val); break;
		case 25 : write_csr(mhpmevent25, val); break;
		case 26 : write_csr(mhpmevent26, val); break;
		case 27 : write_csr(mhpmevent27, val); break;
		case 28 : write_csr(mhpmevent28, val); break;
		case 29 : write_csr(mhpmevent29, val); break;
		case 30 : write_csr(mhpmevent30, val); break;
		case 31 : write_csr(mhpmevent31, val); break;
	};
}

//Read Command

#define READ_COUNTER(n) read_csr(mhpmcounter##n)

#endif

