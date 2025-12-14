#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#define MAX_THREADS 8
const long array_sizes[] = {10000, 1000000, 5000000, 10000000, 50000000};
const int thread_counts[] = {2, 4, 10, 100, 1000};

long long get_time_ms() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        perror("clock_gettime");
        return 0;
    }
    return (long long)ts.tv_sec * 1000 + (long long)ts.tv_nsec / 1000000;
}

typedef struct {
    int *array;
    long start_index;
    long end_index;
    long long partial_sum;
} thread_segment_t;


long long single_thread_sum(int *arr, long N) {
    long long total_sum = 0;
    for (long i = 0; i < N; i++) {
        total_sum += arr[i];
    }
    return total_sum;
}

void* thread_sum_segment(void* arg) {
    thread_segment_t* data = (thread_segment_t*)arg;
    long long current_sum = 0;

    for (long i = data->start_index; i < data->end_index; i++) {
        current_sum += data->array[i];
    }
    data->partial_sum = current_sum;
    return NULL;
}

long long multi_thread_sum(int *arr, long N, int T, long long *total_sum_out) {
    pthread_t threads[T];
    thread_segment_t thread_data[T];
    long chunk_size = N / T;
    long remainder = N % T;
    long current_start = 0;

    for (int i = 0; i < T; i++) {
        long segment_size = chunk_size + (i < remainder ? 1 : 0);
        thread_data[i].array = arr;
        thread_data[i].start_index = current_start;
        thread_data[i].end_index = current_start + segment_size;
        thread_data[i].partial_sum = 0;

        current_start += segment_size;
    }

    long long start_time = get_time_ms();
    for (int i = 0; i < T; i++) {
        if (pthread_create(&threads[i], NULL, thread_sum_segment, &thread_data[i]) != 0) {
            perror("Failed to create thread");
            return -1;
        }
    }
    for (int i = 0; i < T; i++) {
        pthread_join(threads[i], NULL);
    }
    long long end_time = get_time_ms();
    *total_sum_out = 0;
    for (int i = 0; i < T; i++) {
        *total_sum_out += thread_data[i].partial_sum;
    }
    return end_time - start_time;
}

int* initialize_array(long N) {
    int *arr = (int*)malloc(N * sizeof(int));
    if (arr == NULL) {
        return NULL;
    }
    for (long i = 0; i < N; i++) {
        arr[i] = rand() % 10;
    }
    return arr;
}

int init_and_check_array(int **arr_ptr, long N) {
    *arr_ptr = initialize_array(N);
    if (*arr_ptr == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for N=%ld. Skipping.\n", N);
        return 0;
    }
    return 1;
}

void runSingleSum(long N, const int* thread_counts, int num_T) {
    int *arr;
    if (!init_and_check_array(&arr, N)) {
        return;
    }

    long long single_sum;
    long long start_time_single = get_time_ms();
    single_sum = single_thread_sum(arr, N);
    long long single_time = get_time_ms() - start_time_single;
    printf("%ld %lld\n", N, single_time);
    free(arr);
}

void runMultiSum(long N, const int* thread_counts, int num_T) {
    int *arr;
    if (!init_and_check_array(&arr, N)) {
        return;
    }

    long long multi_sum;
    for (int t = 0; t < num_T; t++) {
        int T = thread_counts[t];
        long long multi_time = multi_thread_sum(arr, N, T, &multi_sum);
        printf("%lld ", multi_time);
    }
    printf("\n");
    free(arr);
}

int main() {
    srand(time(NULL));
    int num_sizes = sizeof(array_sizes) / sizeof(array_sizes[0]);
    int num_T = sizeof(thread_counts) / sizeof(thread_counts[0]);
    for (int s = 0; s < num_sizes; s++) {
        printf("Single-Thread Sum for N=%ld:\n", array_sizes[s]);
        runSingleSum(array_sizes[s], thread_counts, num_T);
    }

    printf("\n");
    for(int i = 0; i < num_T; i++) printf("T=%d ", thread_counts[i]);
    printf("\n");
    for (int s = 0; s < num_sizes; s++) {
        printf("Multi-Thread Sum for N  =%ld\n", array_sizes[s]);
        runMultiSum(array_sizes[s], thread_counts, num_T);
    }
    return 0;
}