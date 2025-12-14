#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

long global_counter = 0;
int K_INCREMENTS = 100000;

void *increment_counter_unsafe(void *arg) {
    for (int i = 0; i < K_INCREMENTS; i++) {
        global_counter++;
    }
    return NULL;
}

void run_race_condition_scenario(int M_THREADS) {
    pthread_t *threads = (pthread_t *)malloc(M_THREADS * sizeof(pthread_t));
    printf("Race Condition Scenario\n");
    global_counter = 0;
    for (int i = 0; i < M_THREADS; i++) {
        pthread_create(&threads[i], NULL, increment_counter_unsafe, NULL);
    }
    for (int i = 0; i < M_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    long expected = (long)M_THREADS * K_INCREMENTS;
    printf("Expected Counter Value: %ld\n", expected);
    printf("Actual Counter Value: %ld\n", global_counter);
    printf("Difference: %ld\n", expected - global_counter);
    free(threads);
}

int main() {
    int M_SMALL = 4;
    K_INCREMENTS = 100000;
    run_race_condition_scenario(M_SMALL);
    // run_mutex_safe_scenario(M_SMALL);
    int M_LARGE = 20;
    K_INCREMENTS = 1000000;
    run_race_condition_scenario(M_SMALL);
    // run_mutex_safe_scenario(M_LARGE);
    return 0;
}