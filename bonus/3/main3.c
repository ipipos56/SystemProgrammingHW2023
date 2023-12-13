#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>

#define NUM_LOCKS 10000000 // 10 million locks/unlocks total

pthread_mutex_t mutex;
atomic_int counter = 0;
long long *times; // Dynamic array to store durations of each lock/unlock operation

int cmp(const void *a, const void *b) {
    return (*(long long*)a - *(long long*)b);
}

void *thread_function(void *arg) {
    (void)arg;
    struct timespec start, end;

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        pthread_mutex_lock(&mutex);

        if (counter >= NUM_LOCKS) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        counter++;
        pthread_mutex_unlock(&mutex);
        clock_gettime(CLOCK_MONOTONIC, &end);

        times[counter-1] = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <number_of_threads>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]);
    pthread_t threads[num_threads];
    pthread_mutex_init(&mutex, NULL);

    // Allocate memory for times array
    times = malloc(NUM_LOCKS * sizeof(long long));

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, thread_function, NULL);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Sorting the times array to find min, max, and median
    qsort(times, counter, sizeof(long long), cmp);

    long long min_time = times[0];
    long long max_time = times[counter - 1];
    long long median_time = times[counter / 2];

    printf("Min time: %lld ns, Max time: %lld ns, Median time: %lld ns\n", min_time, max_time, median_time);

    free(times);
    pthread_mutex_destroy(&mutex);

    return 0;
}
