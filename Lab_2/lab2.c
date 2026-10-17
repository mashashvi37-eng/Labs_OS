#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <pthread.h>

static pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static size_t active_threads = 0;
static size_t max_threads_limit = 1;

double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

void merge(int32_t *arr, int32_t low, int32_t mid, int32_t high, int32_t *temp) {
    int32_t i = low, j = mid + 1, k = low;
    while (i <= mid && j <= high) {
        if (arr[i] <= arr[j]) temp[k++] = arr[i++];
        else temp[k++] = arr[j++];
    }
    while (i <= mid) temp[k++] = arr[i++];
    while (j <= high) temp[k++] = arr[j++];
    for (i = low; i <= high; i++) arr[i] = temp[i];
}

void sequential_mergesort(int32_t *arr, int32_t low, int32_t high, int32_t *temp) {
    if (low < high) {
        int32_t mid = low + (high - low) / 2;
        sequential_mergesort(arr, low, mid, temp);
        sequential_mergesort(arr, mid + 1, high, temp);
        merge(arr, low, mid, high, temp);
    }
}

typedef struct {
    int32_t *arr;
    int32_t low;
    int32_t high;
    int32_t *temp;
} ThreadArgs;

static void *parallel_mergesort_thread(void *_args)
{
    ThreadArgs *args = (ThreadArgs *)_args;
    int32_t *arr = args->arr;
    int32_t low = args->low;
    int32_t high = args->high;
    int32_t *temp = args->temp;
    free(args);

    pthread_mutex_lock(&thread_count_mutex);
    active_threads++;
    pthread_mutex_unlock(&thread_count_mutex);

    if (low < high) {
        int32_t mid = low + (high - low) / 2;

        bool can_create = false;

        pthread_mutex_lock(&thread_count_mutex);
        if (active_threads + 1 < max_threads_limit) {
            active_threads += 2;
            can_create = true;
        }
        pthread_mutex_unlock(&thread_count_mutex);

        if (can_create) {
            pthread_t left_thread, right_thread;

            ThreadArgs *left_args = malloc(sizeof(ThreadArgs));
            *left_args = (ThreadArgs){ .arr = arr, .low = low, .high = mid, .temp = temp };
            ThreadArgs *right_args = malloc(sizeof(ThreadArgs));
            *right_args = (ThreadArgs){ .arr = arr, .low = mid + 1, .high = high, .temp = temp };

            pthread_create(&left_thread, NULL, parallel_mergesort_thread, left_args);
            pthread_create(&right_thread, NULL, parallel_mergesort_thread, right_args);

            pthread_join(left_thread, NULL);
            pthread_join(right_thread, NULL);
        } else {
            sequential_mergesort(arr, low, mid, temp);
            sequential_mergesort(arr, mid + 1, high, temp);
        }

        merge(arr, low, mid, high, temp);
    }

    pthread_mutex_lock(&thread_count_mutex);
    active_threads--;
    pthread_mutex_unlock(&thread_count_mutex);

    return NULL;
}

int main(int argc, char **argv)
{
    int32_t N = 1000000;
    size_t single_p = 0;

    if (argc >= 2) {
        if (strcmp(argv[1], "-p") == 0 && argc >= 3) {
            single_p = (size_t)atoi(argv[2]);
        } else {
            N = atoi(argv[1]);
            if (argc >= 3) {
                single_p = (size_t)atoi(argv[2]);
            }
        }
    }
    if (N < 1) N = 1000000;

    int32_t *template_arr = malloc(N * sizeof(int32_t));
    int32_t *arr_seq = malloc(N * sizeof(int32_t));
    int32_t *arr_par = malloc(N * sizeof(int32_t));
    int32_t *temp = malloc(N * sizeof(int32_t));

    srand(42);
    for (int32_t i = 0; i < N; i++) {
        template_arr[i] = rand() % 10000;
    }

    memcpy(arr_seq, template_arr, N * sizeof(int32_t));
    double t0 = get_time_ms();
    sequential_mergesort(arr_seq, 0, N - 1, temp);
    double t1 = get_time_ms();
    double seq_time = t1 - t0;

    size_t test_threads_full[] = {1, 2, 3, 4, 8, 16, 128, 1024};
    size_t test_threads_single[] = {single_p};
    size_t *test_threads = (single_p > 0) ? test_threads_single : test_threads_full;
    size_t num_tests = (single_p > 0) ? 1 : (sizeof(test_threads_full) / sizeof(test_threads_full[0]));

    printf("| Threads      | Time (ms)   | Speedup | Efficiency |\n");
    printf("+--------------+-------------+---------+------------+\n");

    if (single_p == 0) {
        printf("| 1 (seq)      | %11.2f | %7.2f | %10.3f |\n",
               seq_time, 1.0, 1.0);
    }

    for (size_t t = 0; t < num_tests; t++) {
        size_t p = test_threads[t];
        max_threads_limit = p;

        memcpy(arr_par, template_arr, N * sizeof(int32_t));
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        *args = (ThreadArgs){
            .arr = arr_par, .low = 0, .high = N - 1, .temp = temp
        };

        pthread_mutex_lock(&thread_count_mutex);
        active_threads = 0;
        pthread_mutex_unlock(&thread_count_mutex);

        pthread_t main_thread;
        t0 = get_time_ms();
        pthread_create(&main_thread, NULL, parallel_mergesort_thread, args);
        pthread_join(main_thread, NULL);
        t1 = get_time_ms();
        double par_time = t1 - t0;

        double speedup = seq_time / par_time;
        double efficiency = speedup / p;

        printf("| %-12zu | %11.2f | %7.2f | %10.3f |\n",
               p, par_time, speedup, efficiency);
    }

    printf("+--------------+-------------+---------+------------+\n");

    if (single_p > 0) {
        printf("\n=== Sleeping 5 seconds for thread inspection ===\n");
        printf("In another terminal run: ps -eLf | grep lab2\n");
        sleep(5);
    }

    free(template_arr);
    free(arr_seq);
    free(arr_par);
    free(temp);
    return 0;
}
