#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    int thread_id;
    int total_threads;
} thread_data_t;

void* thread_func(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int id = data->thread_id;
    int N = data->total_threads;
    int sleep_seconds = (rand() % 3) + 1;
    printf("Thread %d of %d starting. Sleeping for %d seconds...\n", id, N, sleep_seconds);
    sleep(sleep_seconds);
    printf("I'm thread %d of %d\n", id, N);
    free(data);
    return NULL;
}

int main() {
    int N;
    srand(time(NULL));
    printf("Enter the number of threads : ");
    scanf("%d", &N);
    pthread_t *threads = (pthread_t*)malloc(N * sizeof(pthread_t));
    if (threads == NULL) {
        perror("Failed to allocate memory for threads");
        return 1;
    }
    for (int i = 0; i < N; i++) {
        thread_data_t* data = (thread_data_t*)malloc(sizeof(thread_data_t));
        if (data == NULL) {
            perror("Failed to allocate memory for thread data");
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            free(threads);
            return 1;
        }
        data->thread_id = i + 1;
        data->total_threads = N;
        if (pthread_create(&threads[i], NULL, thread_func, (void*)data) != 0) {
            perror("Failed to create thread");
            free(data);
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            free(threads);
            return 1;
        }
    }
    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }
    free(threads);
    return 0;
}