#include "hpm_profiler.h"

#define setStats profiler_set_stats
#define main benchmark_main
#include "../benchmarks/qsort/qsort_main.c"
#undef main
#undef setStats

int main(int argc, char **argv)
{
    profiler_run_benchmark(benchmark_main, argc, argv, "qsort");
    return 0;
}
