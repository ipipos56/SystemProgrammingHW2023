#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define NUM_ITERATIONS 100000

// Dummy function for thread
void* dummy_function(void* arg) {
    (void)arg;
    return NULL;
}

int cmp(const void *a, const void *b) {
    return (*(long long*)a - *(long long*)b);
}

// Benchmark function for pthread create and join
void benchmark() {
    pthread_t thread;
    struct timespec start, end;
    long long min_time = 1e9, max_time = 0, duration;
    long long total_durations[NUM_ITERATIONS];

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        pthread_create(&thread, NULL, dummy_function, NULL);
        pthread_join(thread, NULL);
        clock_gettime(CLOCK_MONOTONIC, &end); // PER ONE PAIR(as in task)

        duration = (end.tv_sec - start.tv_sec) * 1e9;
        duration += (end.tv_nsec - start.tv_nsec);
        total_durations[i] = duration;

        if (duration < min_time) min_time = duration;
        if (duration > max_time) max_time = duration;
    }

    // Sorting to find the median
    qsort(total_durations, NUM_ITERATIONS, sizeof(long long), cmp);

    printf("Median time per create+join: %lld ns\n", total_durations[NUM_ITERATIONS / 2]);
    printf("Minimum time: %lld ns\n", min_time);
    printf("Maximum time: %lld ns\n", max_time);
}

int main() {
    benchmark();
    return 0;
}
