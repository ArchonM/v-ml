#include "hpm_profiler.h"

#define setStats profiler_set_stats
#define main benchmark_main
#include "../benchmarks/spmv/spmv_main.c"
#undef main
#undef setStats

int main(int argc, char **argv)
{
    profiler_init();
    int ret = benchmark_main(argc, argv);
    profiler_stop();
    profiler_print("spmv");
    return ret;
}
