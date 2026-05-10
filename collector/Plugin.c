#include "HPC.h"
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "median/dataset1.h"
#include "multiply/multiply.c"

#define MTIME_PTR      ((volatile uint32_t*)(CLINT_BASE + 0xBFF8))
#define MTIMECMP_PTR   ((volatile uint32_t*)(CLINT_BASE + 0x4000))

#define MTIE (1 << 7)
#define MIE  (1 << 3)

// Increased buffer size to capture more interrupts over the longer workload
#define BUFFER_SIZE 500 
volatile uint32_t interrupt_count = 0;
volatile uint64_t interval = 100;

// Performance buffers
uint64_t cycle_buffer[BUFFER_SIZE];
uint64_t instret_buffer[BUFFER_SIZE];
// Buffers for all 29 HPM counters (3 through 31)
uint64_t hpm_buffers[29][BUFFER_SIZE];

void __attribute__((interrupt("machine"), aligned(16))) handle_trap(void) {
    uintptr_t cause = read_csr(mcause);
    
    // Check for Machine Timer Interrupt
    if ((cause & 0x7FFFFFFF) == 7) { 
        if (interrupt_count < BUFFER_SIZE) {
            // Snapshot current counters using built-in read_csr
            cycle_buffer[interrupt_count]   = read_csr(mcycle);
            instret_buffer[interrupt_count] = read_csr(minstret);
            
            // Snapshot all configured HPMs
            hpm_buffers[0][interrupt_count] = read_csr(mhpmcounter3);
            hpm_buffers[1][interrupt_count] = read_csr(mhpmcounter4);
            hpm_buffers[2][interrupt_count] = read_csr(mhpmcounter5);
            hpm_buffers[3][interrupt_count] = read_csr(mhpmcounter6);
            hpm_buffers[4][interrupt_count] = read_csr(mhpmcounter7);
            hpm_buffers[5][interrupt_count] = read_csr(mhpmcounter8);
            hpm_buffers[6][interrupt_count] = read_csr(mhpmcounter9);
            hpm_buffers[7][interrupt_count] = read_csr(mhpmcounter10);
            hpm_buffers[8][interrupt_count] = read_csr(mhpmcounter11);
            hpm_buffers[9][interrupt_count] = read_csr(mhpmcounter12);
            hpm_buffers[10][interrupt_count] = read_csr(mhpmcounter13);
            hpm_buffers[11][interrupt_count] = read_csr(mhpmcounter14);
            hpm_buffers[12][interrupt_count] = read_csr(mhpmcounter15);
            hpm_buffers[13][interrupt_count] = read_csr(mhpmcounter16);
            hpm_buffers[14][interrupt_count] = read_csr(mhpmcounter17);
            hpm_buffers[15][interrupt_count] = read_csr(mhpmcounter18);
            hpm_buffers[16][interrupt_count] = read_csr(mhpmcounter19);
            hpm_buffers[17][interrupt_count] = read_csr(mhpmcounter20);
            hpm_buffers[18][interrupt_count] = read_csr(mhpmcounter21);
            hpm_buffers[19][interrupt_count] = read_csr(mhpmcounter22);
            hpm_buffers[20][interrupt_count] = read_csr(mhpmcounter23);
            hpm_buffers[21][interrupt_count] = read_csr(mhpmcounter24);
            hpm_buffers[22][interrupt_count] = read_csr(mhpmcounter25);
            hpm_buffers[23][interrupt_count] = read_csr(mhpmcounter26);
            hpm_buffers[24][interrupt_count] = read_csr(mhpmcounter27);
            hpm_buffers[25][interrupt_count] = read_csr(mhpmcounter28);
            hpm_buffers[26][interrupt_count] = read_csr(mhpmcounter29);
            hpm_buffers[27][interrupt_count] = read_csr(mhpmcounter30);
            hpm_buffers[28][interrupt_count] = read_csr(mhpmcounter31);
        }

        write_csr(mhpmcounter3, 0);
        write_csr(mhpmcounter4, 0);
        write_csr(mhpmcounter5, 0);
        write_csr(mhpmcounter6, 0);
        write_csr(mhpmcounter7, 0);
        write_csr(mhpmcounter8, 0);
        write_csr(mhpmcounter9, 0);
        write_csr(mhpmcounter10, 0);
        write_csr(mhpmcounter11, 0);
        write_csr(mhpmcounter12, 0);
        write_csr(mhpmcounter13, 0);
        write_csr(mhpmcounter14, 0);
        write_csr(mhpmcounter15, 0);
        write_csr(mhpmcounter16, 0);
        write_csr(mhpmcounter17, 0);
        write_csr(mhpmcounter18, 0);
        write_csr(mhpmcounter19, 0);
        write_csr(mhpmcounter20, 0);
        write_csr(mhpmcounter21, 0);
        write_csr(mhpmcounter22, 0);
        write_csr(mhpmcounter23, 0);
        write_csr(mhpmcounter24, 0);
        write_csr(mhpmcounter25, 0);
        write_csr(mhpmcounter26, 0);
        write_csr(mhpmcounter27, 0);
        write_csr(mhpmcounter28, 0);
        write_csr(mhpmcounter29, 0);
        write_csr(mhpmcounter30, 0);
        write_csr(mhpmcounter31, 0);
        
        interrupt_count++;

        // Re-arm timer
        uint32_t lo = MTIME_PTR[0];
        uint32_t hi = MTIME_PTR[1];
        uint64_t next = (((uint64_t)hi << 32) | lo) + interval;

        MTIMECMP_PTR[1] = 0xFFFFFFFF; 
        MTIMECMP_PTR[0] = (uint32_t)next;
        MTIMECMP_PTR[1] = (uint32_t)(next >> 32);
    }
}


void matmul_64x64() {
    static int a[64][64], b[64][64], c[64][64];
    for (int i = 0; i < 64; i++) {
        for (int j = 0; j < 64; j++) {
            int sum = 0.0;
            for (int k = 0; k < 64; k++) {
                sum += a[i][k] * b[k][j];
            }
            c[i][j] = sum;
        }
    }
}

int main(void) {
    // 1. Setup Vector Table
    asm volatile ("csrw mtvec, %0" : : "r"((uintptr_t)handle_trap));
    asm volatile ("csrc mideleg, %0" : : "r"(MTIE));

    HPC_INIT_ALL();

    // 2. Configure HPM counters
    HPC_ASSIGN(3, EVENT_INSTR, HPM_LOAD);
    HPC_ASSIGN(4, EVENT_INSTR, HPM_STORE);
    HPC_ASSIGN(5, EVENT_INSTR, HPM_ARITH);
    HPC_ASSIGN(6, EVENT_INSTR, HPM_EXCEPTION);
    HPC_ASSIGN(7, EVENT_INSTR, HPM_AMO);
    HPC_ASSIGN(8, EVENT_INSTR, HPM_SYS);
    HPC_ASSIGN(9, EVENT_INSTR, HPM_JAL);
    HPC_ASSIGN(10, EVENT_INSTR, HPM_JALR);
    HPC_ASSIGN(11, EVENT_INSTR, HPM_MUL);
    HPC_ASSIGN(12, EVENT_INSTR, HPM_DIV);
    HPC_ASSIGN(13, EVENT_INSTR, HPM_LOAD_FP);
    HPC_ASSIGN(14, EVENT_INSTR, HPM_STORE_FP);
    HPC_ASSIGN(15, EVENT_INSTR, HPM_ADD_FP);
    HPC_ASSIGN(16, EVENT_INSTR, HPM_MUL_FP);
    HPC_ASSIGN(17, EVENT_INSTR, HPM_MADD_FP);
    HPC_ASSIGN(18, EVENT_INSTR, HPM_DIVSQRT_FP);
    HPC_ASSIGN(19, EVENT_INSTR, HPM_OTHER_FP);
    HPC_ASSIGN(20, EVENT_STALL, LOAD_USE_INTLK);
    HPC_ASSIGN(21, EVENT_STALL, LONG_INTLK);
    HPC_ASSIGN(22, EVENT_STALL, CSR_INTLK);
    HPC_ASSIGN(23, EVENT_STALL, ICACHE_BLK);
    HPC_ASSIGN(24, EVENT_STALL, BR_MISPRED);
    HPC_ASSIGN(25, EVENT_STALL, TGT_MISPRED);
    HPC_ASSIGN(26, EVENT_STALL, FLUSH);
    HPC_ASSIGN(27, EVENT_STALL, REPLAY);
    HPC_ASSIGN(28, EVENT_STALL, MUL_DIV_INTLK);
    HPC_ASSIGN(29, EVENT_STALL, FP_INTLK);
    HPC_ASSIGN(30, EVENT_MEM, ICACHE_MISS);
    HPC_ASSIGN(31, EVENT_MEM, ITLB_MISS);

    // Reset counters
    write_csr(mhpmcounter3, 0);
    write_csr(mhpmcounter4, 0);
    write_csr(mhpmcounter5, 0);
    write_csr(mhpmcounter6, 0);
    write_csr(mhpmcounter7, 0);
    write_csr(mhpmcounter8, 0);
    write_csr(mhpmcounter9, 0);
    write_csr(mhpmcounter10, 0);
    write_csr(mhpmcounter11, 0);
    write_csr(mhpmcounter12, 0);
    write_csr(mhpmcounter13, 0);
    write_csr(mhpmcounter14, 0);
    write_csr(mhpmcounter15, 0);
    write_csr(mhpmcounter16, 0);
    write_csr(mhpmcounter17, 0);
    write_csr(mhpmcounter18, 0);
    write_csr(mhpmcounter19, 0);
    write_csr(mhpmcounter20, 0);
    write_csr(mhpmcounter21, 0);
    write_csr(mhpmcounter22, 0);
    write_csr(mhpmcounter23, 0);
    write_csr(mhpmcounter24, 0);
    write_csr(mhpmcounter25, 0);
    write_csr(mhpmcounter26, 0);
    write_csr(mhpmcounter27, 0);
    write_csr(mhpmcounter28, 0);
    write_csr(mhpmcounter29, 0);
    write_csr(mhpmcounter30, 0);
    write_csr(mhpmcounter31, 0);

    // 3. Initial Timer Setup
    uint64_t start = (((uint64_t)MTIME_PTR[1] << 32) | MTIME_PTR[0]);
    uint64_t first = start + interval;
    MTIMECMP_PTR[1] = 0xFFFFFFFF;
    MTIMECMP_PTR[0] = (uint32_t)first;
    MTIMECMP_PTR[1] = (uint32_t)(first >> 32);

    // Enable FPU in mstatus (set FS bits 13:14 to 01, 10, or 11)
    uintptr_t mstatus_val = read_csr(mstatus);
    mstatus_val |= (3 << 13); // Set FS to 'Dirty' (active)
    write_csr(mstatus, mstatus_val);	

    // 4. Enable Interrupts
    write_csr(mie, MTIE);
    write_csr(mstatus, MIE);

    printf("Starting Example Program\n");
    
    // Add test function here (don't forget header)
    for (int i = 0; i < 1000; i ++) {
	matmul_64x64();
    }

// 5. Disable interrupts and print buffers
    write_csr(mie, 0); 

    printf("\nWorkload Finished. Total Interrupts Caught: %u\n", interrupt_count);
    
    // Header with Descriptive Names
    printf("IRQ | Cycles | Instret | Load | Store | Arith | Excpt | AMO | Sys | JAL | JALR | Mul | Div | FLd | FSt | FAdd | FMul | FMadd | FDiv | FOth | LdUse | LngLat | CSR | ICBlk | BrMis | TgtMis | Flush | Replay | MDIntk | FPIntk | ICMiss | TLBMiss\n");
    printf("--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

    uint32_t samples = (interrupt_count > BUFFER_SIZE) ? BUFFER_SIZE : interrupt_count;
    for (uint32_t i = 0; i < samples; i++) {
        // Print standard counters
        printf("%u | %" PRIu64 " | %" PRIu64, 
               i + 1, cycle_buffer[i], instret_buffer[i]);
        
        // Print all 29 HPM buffers
        for (int j = 0; j < 29; j++) {
            printf(" | %" PRIu64, hpm_buffers[j][i]);
        }
        printf("\n");
    }

    if (interrupt_count > BUFFER_SIZE) {
        printf("... (Buffer full: %u additional interrupts not logged)\n", interrupt_count - BUFFER_SIZE);
    }
    return 0;
}

