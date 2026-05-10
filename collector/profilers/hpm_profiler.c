#include "hpm_profiler.h"

#include <inttypes.h>
#include <stdio.h>

#include "../HPC.h"

#define MTIME_PTR      ((volatile uint32_t *)(CLINT_BASE + 0xBFF8))
#define MTIMECMP_PTR   ((volatile uint32_t *)(CLINT_BASE + 0x4000))

#define MACHINE_TIMER_IRQ 7
#define MTIE_BIT (1UL << MACHINE_TIMER_IRQ)

static volatile uint32_t interrupt_count;
static volatile uint32_t profiling_enabled;

static uint64_t cycle_buffer[HPM_PROFILER_BUFFER_SIZE];
static uint64_t instret_buffer[HPM_PROFILER_BUFFER_SIZE];
static uint64_t hpm_buffers[29][HPM_PROFILER_BUFFER_SIZE];

extern void tohost_exit(uintptr_t code);

static uint64_t read_mtime(void)
{
#if __riscv_xlen == 32
    uint32_t hi;
    uint32_t lo;
    uint32_t hi_again;
    do {
        hi = MTIME_PTR[1];
        lo = MTIME_PTR[0];
        hi_again = MTIME_PTR[1];
    } while (hi != hi_again);
    return ((uint64_t)hi << 32) | lo;
#else
    uint32_t lo = MTIME_PTR[0];
    uint32_t hi = MTIME_PTR[1];
    return ((uint64_t)hi << 32) | lo;
#endif
}

static void set_mtimecmp(uint64_t value)
{
    MTIMECMP_PTR[1] = 0xFFFFFFFF;
    MTIMECMP_PTR[0] = (uint32_t)value;
    MTIMECMP_PTR[1] = (uint32_t)(value >> 32);
}

static void arm_timer(void)
{
    set_mtimecmp(read_mtime() + HPM_PROFILER_INTERVAL);
}

static void reset_hpm_counters(void)
{
    write_csr(mcycle, 0);
    write_csr(minstret, 0);
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
}

static void configure_hpm_events(void)
{
    HPC_INIT_ALL();

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
}

static void snapshot_counters(uint32_t index)
{
    cycle_buffer[index] = read_csr(mcycle);
    instret_buffer[index] = read_csr(minstret);
    hpm_buffers[0][index] = read_csr(mhpmcounter3);
    hpm_buffers[1][index] = read_csr(mhpmcounter4);
    hpm_buffers[2][index] = read_csr(mhpmcounter5);
    hpm_buffers[3][index] = read_csr(mhpmcounter6);
    hpm_buffers[4][index] = read_csr(mhpmcounter7);
    hpm_buffers[5][index] = read_csr(mhpmcounter8);
    hpm_buffers[6][index] = read_csr(mhpmcounter9);
    hpm_buffers[7][index] = read_csr(mhpmcounter10);
    hpm_buffers[8][index] = read_csr(mhpmcounter11);
    hpm_buffers[9][index] = read_csr(mhpmcounter12);
    hpm_buffers[10][index] = read_csr(mhpmcounter13);
    hpm_buffers[11][index] = read_csr(mhpmcounter14);
    hpm_buffers[12][index] = read_csr(mhpmcounter15);
    hpm_buffers[13][index] = read_csr(mhpmcounter16);
    hpm_buffers[14][index] = read_csr(mhpmcounter17);
    hpm_buffers[15][index] = read_csr(mhpmcounter18);
    hpm_buffers[16][index] = read_csr(mhpmcounter19);
    hpm_buffers[17][index] = read_csr(mhpmcounter20);
    hpm_buffers[18][index] = read_csr(mhpmcounter21);
    hpm_buffers[19][index] = read_csr(mhpmcounter22);
    hpm_buffers[20][index] = read_csr(mhpmcounter23);
    hpm_buffers[21][index] = read_csr(mhpmcounter24);
    hpm_buffers[22][index] = read_csr(mhpmcounter25);
    hpm_buffers[23][index] = read_csr(mhpmcounter26);
    hpm_buffers[24][index] = read_csr(mhpmcounter27);
    hpm_buffers[25][index] = read_csr(mhpmcounter28);
    hpm_buffers[26][index] = read_csr(mhpmcounter29);
    hpm_buffers[27][index] = read_csr(mhpmcounter30);
    hpm_buffers[28][index] = read_csr(mhpmcounter31);
}

uintptr_t handle_trap(uintptr_t cause, uintptr_t epc, uintptr_t regs[32])
{
    (void)regs;

    uintptr_t interrupt_bit = (uintptr_t)1 << (__riscv_xlen - 1);
    uintptr_t cause_code = cause & ~interrupt_bit;

    if ((cause & interrupt_bit) && cause_code == MACHINE_TIMER_IRQ) {
        if (profiling_enabled && interrupt_count < HPM_PROFILER_BUFFER_SIZE) {
            snapshot_counters(interrupt_count);
        }

        interrupt_count++;
        arm_timer();
        return epc;
    }

    tohost_exit(1337);
    return epc;
}

void profiler_init(void)
{
    configure_hpm_events();
    reset_hpm_counters();
    interrupt_count = 0;
    profiling_enabled = 0;

    write_csr(mideleg, read_csr(mideleg) & ~MTIE_BIT);
    write_csr(mstatus, read_csr(mstatus) | MSTATUS_FS);
}

void profiler_start(void)
{
    interrupt_count = 0;
    profiling_enabled = 1;
    reset_hpm_counters();
    arm_timer();

    write_csr(mie, read_csr(mie) | MTIE_BIT);
    write_csr(mstatus, read_csr(mstatus) | MSTATUS_MIE);
}

void profiler_stop(void)
{
    if (!profiling_enabled) {
        return;
    }

    profiling_enabled = 0;
    write_csr(mie, read_csr(mie) & ~MTIE_BIT);
}

void profiler_set_stats(int enable)
{
    if (enable) {
        profiler_start();
    } else {
        profiler_stop();
    }
}

void profiler_print(const char *benchmark_name)
{
    uint32_t samples = interrupt_count;
    if (samples > HPM_PROFILER_BUFFER_SIZE) {
        samples = HPM_PROFILER_BUFFER_SIZE;
    }

    printf("\nBenchmark: %s\n", benchmark_name);
    printf("Workload Finished. Total Interrupts Caught: %u\n", interrupt_count);
    printf("IRQ | Cycles | Instret | Load | Store | Arith | Excpt | AMO | Sys | JAL | JALR | Mul | Div | FLd | FSt | FAdd | FMul | FMadd | FDiv | FOth | LdUse | LngLat | CSR | ICBlk | BrMis | TgtMis | Flush | Replay | MDIntk | FPIntk | ICMiss | TLBMiss\n");

    for (uint32_t i = 0; i < samples; i++) {
        printf("%u | %" PRIu64 " | %" PRIu64, i + 1, cycle_buffer[i], instret_buffer[i]);
        for (int j = 0; j < 29; j++) {
            printf(" | %" PRIu64, hpm_buffers[j][i]);
        }
        printf("\n");
    }

    if (interrupt_count > HPM_PROFILER_BUFFER_SIZE) {
        printf("... (Buffer full: %u additional interrupts not logged)\n",
               interrupt_count - HPM_PROFILER_BUFFER_SIZE);
    }
}
