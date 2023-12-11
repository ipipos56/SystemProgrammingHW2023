#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define NUM_SIGNALS 1000000
#define NUM_RUNS 5

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int signal_count = 0;
int wait_thread_count = 0;

void *wait_thread(void *arg) {
    (void) arg;
    pthread_mutex_lock(&mutex);
    while (signal_count < NUM_SIGNALS) {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void signal_or_broadcast(int is_broadcast) {
    for (int i = 0; i < NUM_SIGNALS; i++) {
        pthread_mutex_lock(&mutex);
        signal_count++;
        if (is_broadcast) {
            pthread_cond_broadcast(&cond);
        } else {
            pthread_cond_signal(&cond);
        }
        pthread_mutex_unlock(&mutex);
    }
}

long run_test(int wait_threads, int is_broadcast) {
    pthread_t threads[wait_threads];
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    // Start wait threads
    for (int i = 0; i < wait_threads; i++) {
        if (pthread_create(&threads[i], NULL, wait_thread, NULL) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    // Start signaling or broadcasting
    signal_or_broadcast(is_broadcast);

    // Join wait threads
    for (int i = 0; i < wait_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");
            exit(1);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    long elapsed = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
    return elapsed;
}

int compare(const void *a, const void *b) {
    double fa = *(const double*) a;
    double fb = *(const double*) b;
    return (fa > fb) - (fa < fb);
}

void print_stats(long times[], int size) {
    qsort(times, size, sizeof(double), compare);
    printf("Min: %ld ns, Max: %ld ns, Median: %ld ns\n", times[0], times[size - 1], times[size / 2]);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s [num_wait_threads] [signal(0) or broadcast(1)]\n", argv[0]);
        return 1;
    }

    wait_thread_count = atoi(argv[1]);
    int is_broadcast = atoi(argv[2]);
    long times[NUM_RUNS];

    for (int i = 0; i < NUM_RUNS; i++) {
        signal_count = 0;
        times[i] = run_test(wait_thread_count, is_broadcast);
    }

    print_stats(times, NUM_RUNS);

    return 0;
}
