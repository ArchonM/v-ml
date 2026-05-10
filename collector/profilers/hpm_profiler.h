#ifndef HPM_PROFILER_H
#define HPM_PROFILER_H

#include <stdint.h>

#ifndef HPM_PROFILER_BUFFER_SIZE
#define HPM_PROFILER_BUFFER_SIZE 500
#endif

#ifndef HPM_PROFILER_INTERVAL
#define HPM_PROFILER_INTERVAL 100
#endif

void profiler_init(void);
void profiler_start(void);
void profiler_stop(void);
void profiler_set_stats(int enable);
void profiler_print(const char *benchmark_name);

#endif
