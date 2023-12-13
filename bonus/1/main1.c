#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>

#define NUM_ITERATIONS 50000000
#define NUM_RUNS 5

int cmp(const void *a, const void *b) {
    return (*(long long*)a - *(long long*)b);
}

void benchmark_clock(clockid_t clock_id, const char* clock_name) {
    struct timespec start, end;
    long long total_durations[NUM_RUNS];
    long long min_duration = LLONG_MAX, max_duration = 0;

    for (int run = 0; run < NUM_RUNS; ++run) {
        clock_gettime(CLOCK_MONOTONIC, &start);

        for (int i = 0; i < NUM_ITERATIONS; ++i) {
            struct timespec temp;
            clock_gettime(clock_id, &temp);
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        long long duration = ((end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec)) / NUM_ITERATIONS;
        total_durations[run] = duration;
        if (duration < min_duration) min_duration = duration;
        if (duration > max_duration) max_duration = duration;
    }

    // Sorting to find the median
    qsort(total_durations, NUM_RUNS, sizeof(long long), cmp);

    printf("%s: Min: %lld ns, Max: %lld ns, Median: %lld ns (for %d runs)\n",
           clock_name, min_duration, max_duration, total_durations[NUM_RUNS / 2], NUM_RUNS);
}



int main() {
    benchmark_clock(CLOCK_REALTIME, "CLOCK_REALTIME");
    benchmark_clock(CLOCK_MONOTONIC, "CLOCK_MONOTONIC");
    benchmark_clock(CLOCK_MONOTONIC_RAW, "CLOCK_MONOTONIC_RAW");

    return 0;
}
