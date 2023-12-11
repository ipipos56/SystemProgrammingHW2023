#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <stdlib.h>

#define NUM_RUNS 10
#define INCREMENTS 100000000

// Global atomic variable for increment
atomic_int atomic_var = 0;

// Structure to pass parameters to threads
typedef struct {
    int increments;
    int memory_order;
} thread_params;

void* increment_function(void* arg) {
    thread_params *params = (thread_params*) arg;
    for (int i = 0; i < params->increments; i++) {
        if (params->memory_order == 0) { // Relaxed order
            atomic_fetch_add_explicit(&atomic_var, 1, memory_order_relaxed);
        } else { // Sequentially consistent order
            atomic_fetch_add_explicit(&atomic_var, 1, memory_order_seq_cst);
        }
    }
    return NULL;
}

long get_duration_in_ns(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
}

int compare_long(const void* a, const void* b) {
    long arg1 = *(const long*)a;
    long arg2 = *(const long*)b;

    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

void calculate_and_print_stats(long durations[]) {
    qsort(durations, NUM_RUNS, sizeof(long), compare_long);
    long min_duration = durations[0];
    long max_duration = durations[NUM_RUNS - 1];
    long median_duration = durations[NUM_RUNS / 2];

    printf("Min Duration: %ld ns\n", min_duration);
    printf("Max Duration: %ld ns\n", max_duration);
    printf("Median Duration: %ld ns\n\n", median_duration);
}

void run_benchmark(int num_threads, int memory_order) {
    pthread_t threads[num_threads];
    thread_params params = {INCREMENTS / num_threads, memory_order};

    long durations[NUM_RUNS];
    for (int run = 0; run < NUM_RUNS; run++) {
        atomic_var = 0;

        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        for (int i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], NULL, increment_function, &params);
        }

        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        durations[run] = get_duration_in_ns(start, end);
    }

    printf("Benchmark with %d threads and memory order %d:\n", num_threads, memory_order);
    calculate_and_print_stats(durations);
}

int main() {
    // 100 mln increments with 1 thread and relaxed order
    run_benchmark(1, 0);

    // 100 mln increments with 3 threads and relaxed order
    run_benchmark(3, 0);

    // 100 mln increments with 3 threads and sequentially consistent order
    run_benchmark(3, 1);

    return 0;
}
