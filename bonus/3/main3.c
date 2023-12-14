#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>

#define NUM_LOCKS 10000000 // 10 million locks/unlocks total
#define NUM_RUNS 5

pthread_mutex_t mutex;
atomic_int counter = 0;

int cmp(const void *a, const void *b) {
    return (*(long long*)a - *(long long*)b);
}

void *thread_function(void *arg) {
    (void)arg;

    while (1) {
        pthread_mutex_lock(&mutex);

        if (counter >= NUM_LOCKS) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        counter++;
        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <number_of_threads>\n", argv[0]);
        return 1;
    }

    long long *times = malloc(NUM_RUNS * sizeof(long long));

    int num_threads = atoi(argv[1]);
    pthread_t threads[num_threads];
    pthread_mutex_init(&mutex, NULL);


    struct timespec start, end;

    for (int run = 0; run < NUM_RUNS; ++run) {
        counter = 0; // Reset the counter at the start of each run
        clock_gettime(CLOCK_MONOTONIC, &start);

        for (int i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], NULL, thread_function, NULL);
        }

        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        long long duration = ((end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec)) / NUM_LOCKS;
        times[run] = duration;
    }

    // Sorting the times array to find min, max, and median
    qsort(times, NUM_RUNS, sizeof(long long), cmp);

    long long min_time = times[0];
    long long max_time = times[NUM_RUNS - 1];
    long long median_time = times[NUM_RUNS / 2];

    printf("Min time: %lld ns, Max time: %lld ns, Median time: %lld ns\n", min_time, max_time, median_time);

    free(times);
    pthread_mutex_destroy(&mutex);

    return 0;
}
