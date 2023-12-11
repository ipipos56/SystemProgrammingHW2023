#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h>

#define NUM_INCREMENTS 100000000 // 100 million increments
#define MAX_THREADS 3            // Maximum number of threads
#define NUM_RUNS 5         // Number of times to run each benchmark

// Thread argument structure
typedef struct {
    uint64_t *array;
    int index;
} ThreadArg;

void *increment_function(void *arg) {
    ThreadArg *threadArg = (ThreadArg *)arg;
    volatile int dummy = 0; // To prevent loop optimization

    for (uint64_t i = 0; i < NUM_INCREMENTS && !dummy; ++i) {
        threadArg->array[threadArg->index]++;
    }

    return NULL;
}

long long timediff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
}

int compare_ll(const void *a, const void *b) {
    long long arg1 = *(const long long *)a;
    long long arg2 = *(const long long *)b;

    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

void run_benchmark(uint64_t *array, int threadCount, int distance) {
    pthread_t threads[MAX_THREADS];
    ThreadArg args[MAX_THREADS];
    long long durations[NUM_RUNS];

    for (int run = 0; run < NUM_RUNS; ++run) {
        // Prepare threads
        for (int i = 0; i < threadCount; ++i) {
            args[i].array = array;
            args[i].index = i * distance;
            array[args[i].index] = 0;
        }

        // Start timing
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        // Start threads
        for (int i = 0; i < threadCount; ++i) {
            if (pthread_create(&threads[i], NULL, increment_function, &args[i]) != 0) {
                perror("Failed to create thread");
                exit(EXIT_FAILURE);
            }
        }

        // Wait for threads to finish
        for (int i = 0; i < threadCount; ++i) {
            pthread_join(threads[i], NULL);
        }

        // End timing
        clock_gettime(CLOCK_MONOTONIC, &end);

        // Store duration
        durations[run] = timediff(start, end);
    }

    // Calculate min, max, and median
    qsort(durations, NUM_RUNS, sizeof(long long), compare_ll);
    long long min = durations[0];
    long long max = durations[NUM_RUNS - 1];
    long long median = durations[NUM_RUNS / 2];

    // Print results
    printf("Threads: %d, Distance: %d, Min: %lld ns, Max: %lld ns, Median: %lld ns\n",
           threadCount, distance, min, max, median);
}

int main() {
    uint64_t array[MAX_THREADS * 8] = {0}; // Max needed size

    // Run benchmarks
    run_benchmark(array, 1, 1); // 1 thread, close numbers
    run_benchmark(array, 2, 1); // 2 threads, close numbers
    run_benchmark(array, 2, 8); // 2 threads, distant numbers
    run_benchmark(array, 3, 1); // 3 threads, close numbers
    run_benchmark(array, 3, 8); // 3 threads, distant numbers

    return 0;
}
