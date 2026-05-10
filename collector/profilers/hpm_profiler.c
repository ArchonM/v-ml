#include "hpm_profiler.h"

#include <inttypes.h>
#include <stdio.h>

#include "../HPC.h"

#define MTIME_PTR      ((volatile uint32_t *)(CLINT_BASE + 0xBFF8))
#define MTIMECMP_PTR   ((volatile uint32_t *)(CLINT_BASE + 0x4000))

#define MACHINE_TIMER_IRQ 7
#define MTIE_BIT (1UL << MACHINE_TIMER_IRQ)
#define HPM_COUNTER_COUNT 29
#define HPM_EVENT_TOTAL 35

typedef struct {
    const char *name;
    uint32_t set;
    uint32_t mask;
} hpm_event_t;

static const hpm_event_t hpm_events[HPM_EVENT_TOTAL] = {
    {"Load", EVENT_INSTR, HPM_LOAD},
    {"Store", EVENT_INSTR, HPM_STORE},
    {"Arith", EVENT_INSTR, HPM_ARITH},
    {"Branch", EVENT_INSTR, HPM_BRANCH},
    {"Excpt", EVENT_INSTR, HPM_EXCEPTION},
    {"AMO", EVENT_INSTR, HPM_AMO},
    {"Sys", EVENT_INSTR, HPM_SYS},
    {"JAL", EVENT_INSTR, HPM_JAL},
    {"JALR", EVENT_INSTR, HPM_JALR},
    {"Mul", EVENT_INSTR, HPM_MUL},
    {"Div", EVENT_INSTR, HPM_DIV},
    {"FLd", EVENT_INSTR, HPM_LOAD_FP},
    {"FSt", EVENT_INSTR, HPM_STORE_FP},
    {"FAdd", EVENT_INSTR, HPM_ADD_FP},
    {"FMul", EVENT_INSTR, HPM_MUL_FP},
    {"FMadd", EVENT_INSTR, HPM_MADD_FP},
    {"FDiv", EVENT_INSTR, HPM_DIVSQRT_FP},
    {"FOth", EVENT_INSTR, HPM_OTHER_FP},
    {"LdUse", EVENT_STALL, LOAD_USE_INTLK},
    {"LngLat", EVENT_STALL, LONG_INTLK},
    {"CSR", EVENT_STALL, CSR_INTLK},
    {"ICBlk", EVENT_STALL, ICACHE_BLK},
    {"DCBlk", EVENT_STALL, DCACHE_CLK},
    {"BrMis", EVENT_STALL, BR_MISPRED},
    {"TgtMis", EVENT_STALL, TGT_MISPRED},
    {"Flush", EVENT_STALL, FLUSH},
    {"Replay", EVENT_STALL, REPLAY},
    {"MDIntk", EVENT_STALL, MUL_DIV_INTLK},
    {"FPIntk", EVENT_STALL, FP_INTLK},
    {"ICMiss", EVENT_MEM, ICACHE_MISS},
    {"DCMiss", EVENT_MEM, DCACHE_MISS},
    {"DCRel", EVENT_MEM, DCACHE_REL},
    {"ITLBMiss", EVENT_MEM, ITLB_MISS},
    {"DTLBMiss", EVENT_MEM, DTLB_MISS},
    {"L2TLBMiss", EVENT_MEM, L2TB_MISS},
};

#ifdef HPM_PROFILER_EVENT_LIST
static const uint32_t hpm_event_list[] = { HPM_PROFILER_EVENT_LIST };
#endif

static volatile uint32_t interrupt_count;
static volatile uint32_t profiling_enabled;
static volatile uint32_t stats_depth;
static volatile uint32_t repeat_batch_mode;

static uint64_t cycle_buffer[HPM_PROFILER_BUFFER_SIZE];
static uint64_t instret_buffer[HPM_PROFILER_BUFFER_SIZE];
static uint64_t hpm_buffers[HPM_COUNTER_COUNT][HPM_PROFILER_BUFFER_SIZE];
static uint64_t final_hpm_counters[HPM_COUNTER_COUNT];
static uint32_t active_event_count;
static uint64_t start_mtime;
static uint64_t stop_mtime;
static uint64_t start_mcycle;
static uint64_t stop_mcycle;
static uint64_t start_minstret;
static uint64_t stop_minstret;

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

static uint64_t read_hpm_counter(uint32_t counter)
{
    switch (counter) {
        case 3: return read_csr(mhpmcounter3);
        case 4: return read_csr(mhpmcounter4);
        case 5: return read_csr(mhpmcounter5);
        case 6: return read_csr(mhpmcounter6);
        case 7: return read_csr(mhpmcounter7);
        case 8: return read_csr(mhpmcounter8);
        case 9: return read_csr(mhpmcounter9);
        case 10: return read_csr(mhpmcounter10);
        case 11: return read_csr(mhpmcounter11);
        case 12: return read_csr(mhpmcounter12);
        case 13: return read_csr(mhpmcounter13);
        case 14: return read_csr(mhpmcounter14);
        case 15: return read_csr(mhpmcounter15);
        case 16: return read_csr(mhpmcounter16);
        case 17: return read_csr(mhpmcounter17);
        case 18: return read_csr(mhpmcounter18);
        case 19: return read_csr(mhpmcounter19);
        case 20: return read_csr(mhpmcounter20);
        case 21: return read_csr(mhpmcounter21);
        case 22: return read_csr(mhpmcounter22);
        case 23: return read_csr(mhpmcounter23);
        case 24: return read_csr(mhpmcounter24);
        case 25: return read_csr(mhpmcounter25);
        case 26: return read_csr(mhpmcounter26);
        case 27: return read_csr(mhpmcounter27);
        case 28: return read_csr(mhpmcounter28);
        case 29: return read_csr(mhpmcounter29);
        case 30: return read_csr(mhpmcounter30);
        case 31: return read_csr(mhpmcounter31);
        default: return 0;
    }
}

static void configure_hpm_events(void)
{
    HPC_INIT_ALL();

    active_event_count = 0;
#ifdef HPM_PROFILER_EVENT_LIST
    uint32_t configured_event_count = sizeof(hpm_event_list) / sizeof(hpm_event_list[0]);
    for (uint32_t i = 0; i < HPM_COUNTER_COUNT && i < configured_event_count; i++) {
        uint32_t event_index = hpm_event_list[i];
        if (event_index >= HPM_EVENT_TOTAL) {
            continue;
        }

        HPC_ASSIGN((int)(active_event_count + 3), hpm_events[event_index].set, hpm_events[event_index].mask);
        active_event_count++;
    }
#else
    for (uint32_t i = 0; i < HPM_COUNTER_COUNT; i++) {
        uint32_t event_index = HPM_PROFILER_EVENT_OFFSET + i;
        if (i >= HPM_PROFILER_EVENT_COUNT || event_index >= HPM_EVENT_TOTAL) {
            break;
        }

        HPC_ASSIGN((int)(i + 3), hpm_events[event_index].set, hpm_events[event_index].mask);
        active_event_count++;
    }
#endif
}

static uint32_t active_event_index(uint32_t active_index)
{
#ifdef HPM_PROFILER_EVENT_LIST
    return hpm_event_list[active_index];
#else
    return HPM_PROFILER_EVENT_OFFSET + active_index;
#endif
}

static void snapshot_counters(uint32_t index)
{
    cycle_buffer[index] = read_csr(mcycle);
    instret_buffer[index] = read_csr(minstret);
    for (uint32_t i = 0; i < active_event_count; i++) {
        hpm_buffers[i][index] = read_hpm_counter(i + 3);
    }
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
    stats_depth = 0;
    repeat_batch_mode = 0;
    start_mtime = 0;
    stop_mtime = 0;
    start_mcycle = 0;
    stop_mcycle = 0;
    start_minstret = 0;
    stop_minstret = 0;
    for (uint32_t i = 0; i < HPM_COUNTER_COUNT; i++) {
        final_hpm_counters[i] = 0;
    }

    write_csr(mideleg, read_csr(mideleg) & ~MTIE_BIT);
    write_csr(mstatus, read_csr(mstatus) | MSTATUS_FS);
}

void profiler_start(void)
{
    interrupt_count = 0;
    profiling_enabled = 1;
    reset_hpm_counters();
    start_mtime = read_mtime();
    start_mcycle = read_csr(mcycle);
    start_minstret = read_csr(minstret);
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
    stop_mtime = read_mtime();
    stop_mcycle = read_csr(mcycle);
    stop_minstret = read_csr(minstret);
    for (uint32_t i = 0; i < active_event_count; i++) {
        final_hpm_counters[i] = read_hpm_counter(i + 3);
    }
    write_csr(mie, read_csr(mie) & ~MTIE_BIT);
}

void profiler_set_stats(int enable)
{
    if (enable) {
        if (stats_depth == 0 && !profiling_enabled) {
            profiler_start();
        }
        stats_depth++;
    } else {
        if (stats_depth > 0) {
            stats_depth--;
        }
        if (stats_depth == 0 && !repeat_batch_mode) {
            profiler_stop();
        }
    }
}

void profiler_run_benchmark(int (*benchmark_main)(int, char **),
                            int argc,
                            char **argv,
                            const char *benchmark_name)
{
    int ret = 0;

    profiler_init();
    repeat_batch_mode = HPM_PROFILER_REPEATS > 1;
    for (int i = 0; i < HPM_PROFILER_REPEATS; i++) {
        ret = benchmark_main(argc, argv);
        if (ret != 0) {
            break;
        }
    }
    repeat_batch_mode = 0;
    profiler_stop();
    profiler_print(benchmark_name);
    if (ret != 0) {
        tohost_exit((uintptr_t)ret);
    }
}

void profiler_print(const char *benchmark_name)
{
    uint32_t samples = interrupt_count;
    if (samples > HPM_PROFILER_BUFFER_SIZE) {
        samples = HPM_PROFILER_BUFFER_SIZE;
    }
    if (samples > HPM_PROFILER_MAX_PRINT_SAMPLES) {
        samples = HPM_PROFILER_MAX_PRINT_SAMPLES;
    }

    uint32_t print_stride = HPM_PROFILER_PRINT_STRIDE;
    if (print_stride == 0) {
        print_stride = 1;
    }

    printf("\nBenchmark: %s\n", benchmark_name);
    printf("Workload Finished. Total Interrupts Caught: %u\n", interrupt_count);
    printf("Profiler Diagnostics: mtime_delta=%" PRIu64 ", mcycle_delta=%" PRIu64 ", minstret_delta=%" PRIu64 ", interval=%u, repeats=%u, event_offset=%u, active_events=%u, print_stride=%u, max_print_samples=%u\n",
           stop_mtime - start_mtime,
           stop_mcycle - start_mcycle,
           stop_minstret - start_minstret,
           (unsigned)HPM_PROFILER_INTERVAL,
           (unsigned)HPM_PROFILER_REPEATS,
           (unsigned)HPM_PROFILER_EVENT_OFFSET,
           (unsigned)active_event_count,
           (unsigned)print_stride,
           (unsigned)HPM_PROFILER_MAX_PRINT_SAMPLES);

    printf("Event Summary:");
    for (uint32_t i = 0; i < active_event_count; i++) {
        uint32_t event_index = active_event_index(i);
        printf(" %s=%" PRIu64, hpm_events[event_index].name, final_hpm_counters[i]);
    }
    printf("\n");

    if (HPM_PROFILER_SUMMARY_ONLY) {
        return;
    }

    printf("IRQ | Cycles | Instret");
    for (uint32_t i = 0; i < active_event_count; i++) {
        uint32_t event_index = active_event_index(i);
        printf(" | %s", hpm_events[event_index].name);
    }
    printf("\n");

    for (uint32_t i = 0; i < samples; i += print_stride) {
        printf("%u | %" PRIu64 " | %" PRIu64, i + 1, cycle_buffer[i], instret_buffer[i]);
        for (uint32_t j = 0; j < active_event_count; j++) {
            printf(" | %" PRIu64, hpm_buffers[j][i]);
        }
        printf("\n");
    }

    if (interrupt_count > HPM_PROFILER_BUFFER_SIZE) {
        printf("... (Buffer full: %u additional interrupts not logged)\n",
               interrupt_count - HPM_PROFILER_BUFFER_SIZE);
    }
    if (interrupt_count > samples) {
        printf("... (Printing limited to %u captured samples)\n", samples);
    }
}
