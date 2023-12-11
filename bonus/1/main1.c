#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>

#define NUM_ITERATIONS 50000000

int cmp(const void *a, const void *b) {
    return (*(long long *) a - *(long long *) b);
}

void benchmark_clock(clockid_t clock_id, const char *clock_name) {
    struct timespec start, end;
    long long *durations = malloc(NUM_ITERATIONS * sizeof(long long));
    long long min_duration = LLONG_MAX, max_duration = 0;

    // Benchmark loop
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        clock_gettime(clock_id, &start);
        clock_gettime(clock_id, &end);

        long long duration = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
        durations[i] = duration;
        if (duration < min_duration) min_duration = duration;
        if (duration > max_duration) max_duration = duration;
    }

    // Sorting to find the median
    qsort(durations, NUM_ITERATIONS, sizeof(long long), cmp);

    printf("%s: Min: %lld ns, Max: %lld ns, Median: %lld ns\n", clock_name, min_duration, max_duration,
           durations[NUM_ITERATIONS / 2]);

    free(durations);
}

int main() {
    benchmark_clock(CLOCK_REALTIME, "CLOCK_REALTIME");
    benchmark_clock(CLOCK_MONOTONIC, "CLOCK_MONOTONIC");
    benchmark_clock(CLOCK_MONOTONIC_RAW, "CLOCK_MONOTONIC_RAW");

    return 0;
}
