#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>
#include <string.h>

#define NUM_LOCKS 10000000 // 10 million locks/unlocks
#define NUM_RUNS 5 // Number of runs for calculating min, max, and median

pthread_mutex_t mutex;
atomic_int counter = 0;

void *thread_function(void *arg) {
    (void)arg;
    for (int i = 0; i < NUM_LOCKS; i++) {
        pthread_mutex_lock(&mutex);
        counter++;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int compare(const void *a, const void *b) {
    long long a_val = *(const long long *)a;
    long long b_val = *(const long long *)b;
    return (a_val > b_val) - (a_val < b_val);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <number_of_threads>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]);
    pthread_t threads[num_threads];
    pthread_mutex_init(&mutex, NULL);

    long long durations[NUM_RUNS];

    for (int run = 0; run < NUM_RUNS; run++) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        for (int i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], NULL, thread_function, NULL);
        }

        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        durations[run] = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
    }

    qsort(durations, NUM_RUNS, sizeof(long long), compare);

    printf("For %d threads:\n", num_threads);
    printf("Min Duration: %lld ns\n", durations[0]);
    printf("Max Duration: %lld ns\n", durations[NUM_RUNS - 1]);
    printf("Median Duration: %lld ns\n", durations[NUM_RUNS / 2]);

    pthread_mutex_destroy(&mutex);

    return 0;
}
