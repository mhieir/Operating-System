#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

long global_counter = 0;
int K_INCREMENTS = 100000;
pthread_mutex_t counter_mutex;

void *increment_counter_safe(void *arg) {
    for (int i = 0; i < K_INCREMENTS; i++) {
        pthread_mutex_lock(&counter_mutex);
        global_counter++;
        pthread_mutex_unlock(&counter_mutex);
    }
    return NULL;
}

void run_mutex_safe_scenario(int M_THREADS) {
    pthread_t *threads = (pthread_t *)malloc(M_THREADS * sizeof(pthread_t));
    printf("\nMutex Safe Scenario\n");
    global_counter = 0;
    pthread_mutex_init(&counter_mutex, NULL);
    for (int i = 0; i < M_THREADS; i++) {
        pthread_create(&threads[i], NULL, increment_counter_safe, NULL);
    }
    for (int i = 0; i < M_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    long expected = (long)M_THREADS * K_INCREMENTS;
    printf("Expected Counter Value: %ld\n", expected);
    printf("Actual Counter Value: %ld\n", global_counter);
    printf("Difference: %ld\n", expected - global_counter);
    pthread_mutex_destroy(&counter_mutex);
    free(threads);
}

int main() {
    int M_SMALL = 4;
    K_INCREMENTS = 100000;
    run_mutex_safe_scenario(M_SMALL);
    int M_LARGE = 20;
    K_INCREMENTS = 1000000;
    run_mutex_safe_scenario(M_LARGE);
    return 0;
}