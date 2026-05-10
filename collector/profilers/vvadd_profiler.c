#include "hpm_profiler.h"

#define setStats profiler_set_stats
#define main benchmark_main
#include "../benchmarks/vvadd/vvadd_main.c"
#undef main
#undef setStats

int main(int argc, char **argv)
{
    profiler_init();
    int ret = benchmark_main(argc, argv);
    profiler_stop();
    profiler_print("vvadd");
    return ret;
}
