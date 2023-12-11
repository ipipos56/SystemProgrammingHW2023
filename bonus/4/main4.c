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

// Benchmark function for pthread create and join
void benchmark() {
    pthread_t thread;
    struct timespec start, end;
    double min_time = 1e9, max_time = 0, total_time = 0, duration;

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        pthread_create(&thread, NULL, dummy_function, NULL);
        pthread_join(thread, NULL);
        clock_gettime(CLOCK_MONOTONIC, &end);

        duration = (end.tv_sec - start.tv_sec) * 1e9;
        duration += (end.tv_nsec - start.tv_nsec);
        total_time += duration;

        if (duration < min_time) min_time = duration;
        if (duration > max_time) max_time = duration;
    }

    printf("Average time per create+join: %.2f ns\n", total_time / NUM_ITERATIONS);
    printf("Minimum time: %.2f ns\n", min_time);
    printf("Maximum time: %.2f ns\n", max_time);
}

int main() {
    benchmark();
    return 0;
}
