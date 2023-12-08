#include "thread_pool.h"
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>

// Structure definition for a single task in the thread pool.
struct thread_task {
    thread_task_f function;  // Function pointer to the task's function.
    void *arg;               // Argument to be passed to the task's function.

    struct thread_pool *pool;  // Pointer to the thread pool to which this task belongs.

    bool is_detached;   // Flag indicating whether the task is detached.
    bool is_running;    // Flag indicating whether the task is currently running.
    bool is_finished;   // Flag indicating whether the task has finished execution.
    void *result;       // Result of the task's function.

    pthread_cond_t cv;       // Condition variable for task synchronization.
    pthread_mutex_t mutex;   // Mutex for protecting task state.

    struct thread_task *next;  // Pointer to the next task in the task queue.
};

// Structure definition for the thread pool.
struct thread_pool {
    pthread_t *threads;  // Dynamic array of threads in the pool.

    pthread_cond_t cv;         // Condition variable for signaling worker threads.
    pthread_cond_t spawn_cv;   // Condition variable for signaling the creation of new threads.
    pthread_mutex_t mutex;     // Mutex for protecting thread pool state.

    int pending_task_count;   // Count of tasks pending execution.
    int running_task_count;   // Count of tasks currently executing.
    struct thread_task *pending_task_list;  // List of pending tasks.

    int max_thread_count;      // Maximum number of threads allowed in the pool.
    int thread_count;          // Current number of threads in the pool.
    int idle_thread_count;     // Number of idle threads in the pool.

    int thread_idx_that_should_exit;  // Index of the thread that should exit.
};

// Structure for passing arguments to the worker thread function.
struct _thread_worker_arg {
    struct thread_pool *pool;  // Pointer to the thread pool.
    int idx;                   // Index of the worker thread in the pool.
};

// Worker function for threads in the thread pool.
// Responsible for executing tasks from the pool.
static void *_thread_worker(void *arg) {
    struct _thread_worker_arg *args = arg;
    struct thread_pool *pool = args->pool;
    int idx = args->idx;

    free(args);  // Freeing the worker arguments as they are no longer needed.


    pthread_mutex_lock(&pool->mutex);  // Lock the pool mutex for thread-safe operations.
    ++pool->thread_count;              // Increment the thread count.
    pthread_cond_signal(&pool->spawn_cv);  // Signal that a new thread is ready.

    while (true) {
        ++pool->idle_thread_count;  // Increment the count of idle threads.
        // Wait for tasks or for the signal to exit.
        while (pool->pending_task_count < 1 &&
               pool->thread_idx_that_should_exit != idx) {
            pthread_cond_wait(&pool->cv, &pool->mutex);
        }

        if (pool->thread_idx_that_should_exit == idx) {
            pthread_mutex_unlock(&pool->mutex);  // Unlock the pool mutex.
            break;  // Exit the loop if the thread is signaled to terminate.
        }

        // Dequeue a task from the pending tasks list.
        struct thread_task *task = pool->pending_task_list;
        pool->pending_task_list = task->next;
        task->next = NULL;

        --pool->idle_thread_count;      // Decrement idle thread count.
        --pool->pending_task_count;     // Decrement pending task count.
        ++pool->running_task_count;     // Increment running task count.

        pthread_mutex_unlock(&pool->mutex);  // Unlock the pool mutex before executing the task.

        task->is_running = true;  // Mark the task as running.
        void *result = task->function(task->arg);  // Execute the task function.

        // Lock both pool and task mutexes for updating task and pool state.
        pthread_mutex_lock(&pool->mutex);
        pthread_mutex_lock(&task->mutex);
        --pool->running_task_count;  // Decrement running task count.

        // Handling task deletion or marking as finished based on its detached state.
        if (task->is_detached) {
            thread_task_delete(task);  // Delete detached task.
        } else {
            task->is_running = false;
            task->is_finished = true;
            task->result = result;
            pthread_cond_signal(&task->cv);
        }
        pthread_mutex_unlock(&task->mutex);
    }
    return NULL;
}

/* Function: thread_pool_new
 * -------------------------
 * Creates a new thread pool with a specified maximum number of threads.
 *
 * max_thread_count: The maximum number of worker threads in the pool.
 * pool: Pointer to the newly created thread pool.
 *
 * Returns: Integer status code (0 for success, non-zero for errors).
 */
int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
    // Validate the maximum thread count
    if (max_thread_count < 1 || max_thread_count > TPOOL_MAX_THREADS) {
        return TPOOL_ERR_INVALID_ARGUMENT;
    }

    // Allocate memory for the new thread pool
    struct thread_pool *new_pool = malloc(sizeof(struct thread_pool));
    // Initialize thread pool properties
    new_pool->threads = NULL;
    new_pool->max_thread_count = max_thread_count;
    new_pool->thread_count = 0;
    new_pool->idle_thread_count = 0;
    new_pool->thread_idx_that_should_exit = -1;
    new_pool->pending_task_count = 0;
    new_pool->running_task_count = 0;
    new_pool->pending_task_list = NULL;
    // Initialize synchronization primitives
    pthread_mutex_init(&new_pool->mutex, NULL);
    pthread_cond_init(&new_pool->cv, NULL);
    pthread_cond_init(&new_pool->spawn_cv, NULL);

    // Set the output parameter to the new pool
    *pool = new_pool;

    return 0;
}

/* Function: thread_pool_thread_count
 * ----------------------------------
 * Returns the current number of threads in the thread pool.
 *
 * pool: The thread pool to query.
 *
 * Returns: The number of threads currently in the pool.
 */
int
thread_pool_thread_count(const struct thread_pool *pool)
{
    return pool->thread_count;
}

/* Function: thread_pool_delete
 * ----------------------------
 * Cleans up and deletes a thread pool. Ensures that all tasks are completed
 * or not running before deletion.
 *
 * pool: The thread pool to delete.
 *
 * Returns: Integer status code (0 for success, non-zero for errors).
 */
int
thread_pool_delete(struct thread_pool *pool)
{
    // Lock the mutex to ensure thread-safe access
    pthread_mutex_lock(&pool->mutex);
    // Check if any tasks are running or pending
    if (pool->running_task_count > 0 || pool->pending_task_count > 0 || pool->idle_thread_count != pool->thread_count) {
        pthread_mutex_unlock(&pool->mutex);
        return TPOOL_ERR_HAS_TASKS;
    }

    // Clean up worker threads
    if (pool->threads != NULL) {
        // Signal each worker thread to exit
        for (int i = 0; i < pool->thread_count; ++i) {
            pool->thread_idx_that_should_exit = i;
            pthread_cond_broadcast(&pool->cv);
            pthread_mutex_unlock(&pool->mutex);
            pthread_join(pool->threads[i], NULL);
            pthread_mutex_lock(&pool->mutex);
        }
        free(pool->threads);
    }
    // Release the mutex and destroy synchronization primitives
    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cv);
    pthread_cond_destroy(&pool->spawn_cv);

    // Free the pool memory
    free(pool);

    return 0;
}

/* Function: thread_pool_push_task
 * -------------------------------
 * Adds a new task to the thread pool. If necessary, creates new worker threads
 * up to the maximum limit set in the pool.
 *
 * pool: The thread pool to add the task to.
 * task: The task to add to the pool.
 *
 * Returns: Integer status code (0 for success, non-zero for errors).
 */
int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
    // Lock the mutex for thread-safe access
    pthread_mutex_lock(&pool->mutex);
    // Check if the task limit has been reached
    if (pool->pending_task_count >= TPOOL_MAX_TASKS) {
        pthread_mutex_unlock(&pool->mutex);
        return TPOOL_ERR_TOO_MANY_TASKS;
    }

    int required_thread_count = pool->thread_count;

    // Create the first worker if no threads exist
    if (pool->threads == NULL) {
        // Create first worker
        pool->threads = malloc(sizeof(pthread_t));
        struct _thread_worker_arg *arg = malloc(sizeof(struct _thread_worker_arg));
        arg->pool = pool;
        arg->idx = 0;
        required_thread_count = 1;
        pthread_create(&pool->threads[0], NULL, _thread_worker, arg);
    } else if (pool->idle_thread_count == 0 &&
               pool->thread_count < pool->max_thread_count) {
        // Create additional worker
        pool->threads =
                realloc(pool->threads, sizeof(pthread_t) * (pool->thread_count + 1));
        struct _thread_worker_arg *arg = malloc(sizeof(struct _thread_worker_arg));
        arg->pool = pool;
        arg->idx = pool->thread_count;
        pthread_create(&pool->threads[pool->thread_count], NULL, _thread_worker,
                       arg);
        ++required_thread_count;
    }

    while (pool->thread_count != required_thread_count) {
        // Wait until spawned threads will be ready
        pthread_cond_wait(&pool->spawn_cv, &pool->mutex);
    }

    task->pool = pool;
    task->next = pool->pending_task_list;
    pool->pending_task_list = task;
    task->is_finished = false;
    task->is_running = false;
    ++pool->pending_task_count;
    pthread_cond_signal(&pool->cv);
    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
    struct thread_task *new_task = malloc(sizeof(struct thread_task));
    new_task->function = function;
    new_task->arg = arg;
    new_task->pool = NULL;
    new_task->is_detached = false;
    new_task->is_running = false;
    new_task->is_finished = false;
    new_task->result = NULL;
    new_task->next = NULL;
    pthread_mutex_init(&new_task->mutex, NULL);
    pthread_cond_init(&new_task->cv, NULL);

    *task = new_task;

    return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
    return task->is_finished;
}

bool
thread_task_is_running(const struct thread_task *task)
{
    return task->is_running;
}

int
thread_task_join(struct thread_task *task, void **result)
{
    if (task->pool == NULL) {
        return TPOOL_ERR_TASK_NOT_PUSHED;
    }
    pthread_mutex_lock(&task->mutex);
    while (!task->is_finished) {
        pthread_cond_wait(&task->cv, &task->mutex);
    }
    *result = task->result;
    task->pool = NULL;
    pthread_mutex_unlock(&task->mutex);

    return 0;
}

#ifdef NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
    if (task->pool == NULL) {
        return TPOOL_ERR_TASK_NOT_PUSHED;
    }

    pthread_mutex_lock(&task->mutex);

    if (timeout < 0.000000001) {
        if (!task->is_finished) {
            pthread_mutex_unlock(&task->mutex);
            return TPOOL_ERR_TIMEOUT;
        }
    } else {
        long timeout_sec = (long)timeout;
        long timeout_nsec = (long)((timeout - (double)timeout_sec) * 1000000000);
        struct timeval tv;
        struct timespec ts;
        gettimeofday(&tv, NULL);
        long total_nsec = tv.tv_usec * 1000 + timeout_nsec;
        long total_sec = tv.tv_sec + timeout_sec + total_nsec / 1000000000;
        total_nsec %= 1000000000;

        ts.tv_sec = total_sec;
        ts.tv_nsec = total_nsec;

        int cond_res = 0;
        while (!task->is_finished && cond_res != ETIMEDOUT) {
            cond_res = pthread_cond_timedwait(&task->cv, &task->mutex, &ts);
        }

        if (cond_res == ETIMEDOUT) {
            pthread_mutex_unlock(&task->mutex);
            return TPOOL_ERR_TIMEOUT;
        }
    }

    *result = task->result;
    task->pool = NULL;
    pthread_mutex_unlock(&task->mutex);

    return 0;
}

#endif

int
thread_task_delete(struct thread_task *task)
{
    if (task->pool != NULL) {
        return TPOOL_ERR_TASK_IN_POOL;
    }
    pthread_mutex_destroy(&task->mutex);
    pthread_cond_destroy(&task->cv);
    free(task);
    return 0;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
    if (task->pool == NULL) {
        return TPOOL_ERR_TASK_NOT_PUSHED;
    }
    pthread_mutex_lock(&task->mutex);
    task->pool = NULL;
    if (task->is_finished) {
        pthread_mutex_unlock(&task->mutex);
        thread_task_delete(task);
        return 0;
    }
    task->is_detached = true;
    pthread_mutex_unlock(&task->mutex);
    return 0;
}

#endif
