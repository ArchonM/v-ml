#ifndef HPM_PROFILER_H
#define HPM_PROFILER_H

#include <stdint.h>

#ifndef HPM_PROFILER_BUFFER_SIZE
#define HPM_PROFILER_BUFFER_SIZE 500
#endif

#ifndef HPM_PROFILER_INTERVAL
#define HPM_PROFILER_INTERVAL 100
#endif

#ifndef HPM_PROFILER_REPEATS
#define HPM_PROFILER_REPEATS 1
#endif

#ifndef HPM_PROFILER_EVENT_OFFSET
#define HPM_PROFILER_EVENT_OFFSET 0
#endif

#ifndef HPM_PROFILER_EVENT_COUNT
#define HPM_PROFILER_EVENT_COUNT 29
#endif

#ifndef HPM_PROFILER_PRINT_STRIDE
#define HPM_PROFILER_PRINT_STRIDE 1
#endif

#ifndef HPM_PROFILER_MAX_PRINT_SAMPLES
#define HPM_PROFILER_MAX_PRINT_SAMPLES HPM_PROFILER_BUFFER_SIZE
#endif

#ifndef HPM_PROFILER_SUMMARY_ONLY
#define HPM_PROFILER_SUMMARY_ONLY 0
#endif

void profiler_init(void);
void profiler_start(void);
void profiler_stop(void);
void profiler_set_stats(int enable);
void profiler_print(const char *benchmark_name);
void profiler_run_benchmark(int (*benchmark_main)(int, char **),
                            int argc,
                            char **argv,
                            const char *benchmark_name);

#endif
