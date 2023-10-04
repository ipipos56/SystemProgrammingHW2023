#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcoro.h"
#include <time.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>

//DEBUG
//_________________________
//If you want to see all info about coroutines process you can set 1
//But execution speed will be less
#define PRINT_INFO 0

//If you want to see that there is no memory leaks you can set 1 to check it
#define CHECK_LEAKS 0
//_________________________


/**
 * For checking you just simply can run shell script which I provided
 *
 * $> ./test_script.sh
 */

/**
 * WITH CHECK_LEAKS = 1
 * You can compile and run this code using the commands:
 *
 * $> gcc ./utils/heap_help/heap_help.c libcoro.c solution.c
 * $> HHREPORT=v ./a.out 100 3 test1.txt test2.txt test3.txt test4.txt test5.txt test6.txt
 * $> HHREPORT=l ./a.out 100 3 test1.txt test2.txt test3.txt test4.txt test5.txt test6.txt
 */

/**
 * Base running
 * You can compile and run this code using the commands:
 *
 * $> gcc libcoro.c solution.c
 * $> ./a.out 100 3 test1.txt test2.txt test3.txt test4.txt test5.txt test6.txt
 * $> ./a.out 100 6 test1.txt test2.txt test3.txt test4.txt test5.txt test6.txt
 */

#if CHECK_LEAKS == 1
#include "utils/heap_help/heap_help.h"
#endif


void print(const char *format, ...) {
#if PRINT_INFO == 1
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#else
    (void) format;
#endif
}

// file structure
struct file{
    int *data;
    long size;
};


// file_storage structure
struct file_storage {
    char **paths;
    struct file **filesData;
    int cur_unsorted;
    int count;
    int capacity; // all memory capacity
};


// Allocating new file_storage
static struct file_storage *
file_storage_new(void) {
    struct file_storage *list = malloc(sizeof(struct file_storage));
    list->paths = (char **) malloc(10 * sizeof(char *));
    list->capacity = 10;
    list->cur_unsorted = 0;
    list->count = 0;
    return list;
}

// Add a path to the file_storage
void addPath(struct file_storage *list, const char *path) {
    if (list->count >= list->capacity) {
        // Double the capacity if we're out of space
        list->capacity *= 2;
        list->paths = realloc(list->paths, list->capacity * sizeof(char *));
    }
    list->paths[list->count] = strdup(path); // Duplicate the string to store in the list
    list->count++;
}

// checking that we have files unsorted in file_storage
int haveFiles(struct file_storage *list) {
    if (list->cur_unsorted < list->count)
        return list->cur_unsorted;
    else
        return -1;
}


// getting last unsorted index in file_storage
char *getLastUnsortFile(struct file_storage *list) {
    list->cur_unsorted++;
    return list->paths[list->cur_unsorted - 1];
}

// cleaning file_storage to have no memory leaks
void fileStorageCleanup(struct file_storage *list) {
    for (int i = 0; i < list->count; i++) {
        free(list->paths[i]);
    }
    free(list->paths);
    free(list);
}


// cleaning file to have no memory leaks
void fileFree(struct file *fileInfo) {
    if(fileInfo->data)
        free(fileInfo->data);
    free(fileInfo);
}


// creating new empty file
static struct file *
file_new() {
    struct file *fileInfo = malloc(sizeof(struct file));
    fileInfo->data = NULL;
    fileInfo->size = 0;
    return fileInfo;
}

// Data structure for holding coroutine state
struct my_context {
    char *name;
    struct file_storage *files;
    char *filename;
    struct file *curData;
    int64_t work_time; // in nanoseconds
    long time_quantum;
    struct timespec start_time;
    struct timespec end_time;
};

//creating basic my_context which will store information about coroutine
static struct my_context *
my_context_new(const char *name, long time_quantum, struct file_storage *f_stor) {
    struct my_context *ctx;
    // Allocate memory for the structure on the heap.
    ctx = (struct my_context *) malloc(sizeof(struct my_context));
    ctx->name = strdup(name);
    ctx->files = f_stor;
    ctx->filename = "";
    ctx->time_quantum = time_quantum;
    ctx->curData = NULL;
    return ctx;
}

// Calculate the difference between two timespecs in nanoseconds
int64_t calculate_time_difference(struct timespec start, struct timespec end) {
    int64_t start_nanoseconds = (int64_t) start.tv_sec * 1e9 + start.tv_nsec;
    int64_t end_nanoseconds = (int64_t) end.tv_sec * 1e9 + end.tv_nsec;

    return end_nanoseconds - start_nanoseconds;
}

// min function between two integer numbers
int min(int a, int b) {
    return a < b ? a : b;
}

// Function to read file content into an array
void read_file_content(struct my_context *fdata, int dataInd) {
    print("%s\n", fdata->filename);
    FILE *fp = fopen(fdata->filename, "r");
    if (!fp) {
        perror("Error opening file");
        exit(1);
    }

    // Count number of integers in the file
    int count = 0;
    int tmp;
    while (fscanf(fp, "%d", &tmp) == 1) {
        count++;
    }
    rewind(fp);  // Reset file pointer to the beginning

    // Allocate memory for data and read integers into the array
    struct file *tempFile = file_new();
    tempFile->data = (int *) malloc(count * sizeof(int));
    tempFile->size = count;
    print("Counted %d integers in file: %s\n", count, fdata->filename);
    for (int i = 0; i < count; i++) {
        fscanf(fp, "%d", &(tempFile->data[i]));
    }
    fdata->curData = tempFile;
    fdata->files->filesData[dataInd] = tempFile;
    print("Setting size to %zu for file: %s\n", fdata->files->filesData[dataInd]->size, fdata->filename);

    fclose(fp);
}

// Swap function for integers
void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

// Calculating coroutine time from last yield or from start of coroutine
void calculate_coroutine_time(struct my_context *ctx) {
    clock_gettime(CLOCK_MONOTONIC, &(ctx->end_time));
    ctx->work_time += calculate_time_difference(ctx->start_time, ctx->end_time);
}

// yield function for making appropriate coroutine interruptions and time calculating
void yield(struct coro *temp_ctx, char *name, struct my_context *ctx) {
    clock_gettime(CLOCK_MONOTONIC, &(ctx->end_time));
    int64_t elapsed_time = calculate_time_difference(ctx->start_time, ctx->end_time);
    print("Elapsed time of %s is: %" PRId64 ", start_time: %ld, end_time: %ld\n", name, elapsed_time,
         ctx->start_time.tv_nsec, ctx->end_time.tv_nsec);
    if (elapsed_time < ctx->time_quantum) {
        return;
    }
    print("%s: switch count %lld\n", name, coro_switch_count(temp_ctx));
    print("%s: yield\n", name);
    calculate_coroutine_time(ctx);
    coro_yield();
    clock_gettime(CLOCK_MONOTONIC, &(ctx->start_time));
}


// Partition function for quicksort
int partition(int arr[], int low, int high, struct coro *temp_ctx, char *name, struct my_context *ctx) {
    int pivot = arr[high];
    int i = (low - 1);

    for (int j = low; j <= high - 1; j++) {
        if (arr[j] < pivot) {
            i++;
            swap(&arr[i], &arr[j]);
        }
        // Yield after each iteration
        yield(temp_ctx, name, ctx);
    }
    swap(&arr[i + 1], &arr[high]);
    return (i + 1);
}

// Function for printing array
void printArray(int arr[], int size) {
    print("Sorted array: ");
    for (int i = 0; i < size; i++)
        print("%d ", arr[i]);
    print("\n");
}

// Quicksort function
void quicksort(int arr[], int low, int high, struct coro *temp_ctx, char *name, struct my_context *ctx) {
    if (low < high) {
        int pi = partition(arr, low, high, temp_ctx, name, ctx);
        quicksort(arr, low, pi - 1, temp_ctx, name, ctx);
        quicksort(arr, pi + 1, high, temp_ctx, name, ctx);

    }
}


// Merge sorted files into a single file
void merge_sorted_files(struct my_context **file_data_list, int num_files,
                        const char *output_filename) {

    FILE *output_file = fopen(output_filename, "w");
    if (!output_file) {
        perror("Error opening output file");
        exit(1);
    }

    // Temporary array to keep track of current indices in all sorted arrays
    long *indices = (long *) malloc(num_files * sizeof(long));
    for (int i = 0; i < num_files; i++) {
        indices[i] = 0;
    }

    print("Final array: ");
    int done = 0;
    while (!done) {
        done = 1;
        int min_val = INT_MAX;
        int min_ind = -1;

        for (int i = 0; i < num_files; i++) {
            if (indices[i] < file_data_list[0]->files->filesData[i]->size) {
                done = 0;  // There's still some data to merge
                if (file_data_list[0]->files->filesData[i]->data[indices[i]] < min_val) {
                    min_val = file_data_list[0]->files->filesData[i]->data[indices[i]];
                    min_ind = i;
                }
            }
        }

        // Write the minimum value to the output file and update the index
        if (!done) {
            print("%d ", min_val);
            fprintf(output_file, "%d ", min_val);
            indices[min_ind]++;
        }
    }
    print("\n");

    free(indices);
    fclose(output_file);
}

/**
 * Coroutine body. This code is executed by all the coroutines. Here you
 * implement your solution, sort each individual file.
 */
static int
coroutine_func_f(void *context) {
    struct coro *this = coro_this();
    struct my_context *ctx = context;
    char *name = ctx->name;
    int curInd = haveFiles(ctx->files);
    while (curInd != -1) {
        char *filename = getLastUnsortFile(ctx->files);
        ctx->filename = filename;

        clock_gettime(CLOCK_MONOTONIC, &(ctx->start_time));
        print("Started coroutine %s with file %s\n", name, filename);

        // Read file content
        read_file_content(ctx, curInd);


        // Sort the content
        quicksort(ctx->curData->data, 0, ctx->curData->size - 1, this, name, ctx);

        printArray(ctx->curData->data, ctx->curData->size);

        // Save the sorted data back to the file
        //FILE *fp = fopen(ctx->filename, "w");
        //if (!fp) {
        //    perror("Error opening file for writing");
        //    exit(1);
        //}
        //for (size_t i = 0; i < ctx->curData->size; i++) {
        //    fprintf(fp, "%d ", ctx->curData->data[i]);
        //}
        //fclose(fp);
        curInd = haveFiles(ctx->files);
    }

    printf("Total switch count for coroutine %s: %lld\n", name, coro_switch_count(this));
    calculate_coroutine_time(ctx);

    return 0;
}

int main(int argc, char **argv) {
#if CHECK_LEAKS == 1
    heaph_get_alloc_count();
#endif
    // Check for minimum number of arguments
    if (argc < 4) {
        printf("Usage: %s <target_latency> <coroutines_num> <files...>\n", argv[0]);
        return 1;
    }


    struct file_storage *f_stor;
    f_stor = file_storage_new();

    long target_latency = atol(argv[1]) * 1000;
    int coroutines_num = atoi(argv[2]);
    int num_files = argc - 3;
    long time_quantum = target_latency / coroutines_num;


    for (int i = 3; i < argc; i++) {
        addPath(f_stor, argv[i]);
        print("%s\n", argv[i]);
    }

    print("Number of files: %d, All capacity of Files Storage: %d, Current Unsorted index: %d\n", f_stor->count,
          f_stor->capacity, f_stor->cur_unsorted);

    f_stor->filesData = (struct file **) malloc(f_stor->count * sizeof(struct file*));

    struct timespec total_start_time, total_end_time;
    clock_gettime(CLOCK_MONOTONIC, &total_start_time);

    /* Initialize our coroutine global cooperative scheduler. */
    coro_sched_init();

    struct my_context **m_ctxs = (struct my_context **) malloc(num_files * sizeof(struct my_context *));
    /* Start several coroutines. */
    for (int i = 0; i < min(coroutines_num, f_stor->count); ++i) {
        if (haveFiles(f_stor) != -1) {
            struct my_context *m_ctx;
            char name[30];
            sprintf(name, "coro_%d", i);
            m_ctx = my_context_new(name, time_quantum, f_stor);
            m_ctxs[i] = m_ctx;
            print("coro_%d is starting\n", i);
            coro_new(coroutine_func_f, m_ctx);
            //print("Size of file %s: %zu\n", m_ctx->name, m_ctx->size);
        }
    }
    /* Wait for all the coroutines to end. */
    struct coro *c;
    while ((c = coro_sched_wait()) != NULL) {
        print("Finished status: %d\n", coro_status(c));
        coro_delete(c);
    }
    /* All coroutines have finished. */
    clock_gettime(CLOCK_MONOTONIC, &total_end_time);
    int64_t total_time = calculate_time_difference(total_start_time, total_end_time) / 1000;
    print("Total execution time before merging files: %" PRId64 " microseconds\n", total_time);


    // Merge sorted files
    merge_sorted_files(m_ctxs, num_files, "output.txt");

    //Print time for each coroutine
    for (int i = 0; i < min(coroutines_num, f_stor->count); ++i) {
        printf("Total execution time for coroutine %s: %" PRId64 " microseconds\n", m_ctxs[i]->name,
               m_ctxs[i]->work_time / 1000);
    }

    clock_gettime(CLOCK_MONOTONIC, &total_end_time);
    total_time = calculate_time_difference(total_start_time, total_end_time) / 1000;
    printf("Total execution time: %" PRId64 " microseconds\n", total_time);

    // Cleanup
    for (int i = 0; i < min(coroutines_num, f_stor->count); i++) {
        //fileFree(m_ctxs[i]->curData);
        free(m_ctxs[i]->name);
        free(m_ctxs[i]);
    }
    free(m_ctxs);
    for(int i = 0;i<num_files;i++)
        fileFree(f_stor->filesData[i]);
    free(f_stor->filesData);
    fileStorageCleanup(f_stor);

    fflush(stdout);
#if CHECK_LEAKS == 1
    heaph_get_alloc_count();
#endif
    return 0;
}
