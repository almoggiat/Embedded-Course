#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sysmond.h"

void get_data(SystemData* data) {
    int mem[5];
    double cpu[3];
    int process_count;
    bool mem_ready;
    bool cpu_ready;
    bool proc_ready;

    // Copy the shared values while holding the mutex.
    pthread_mutex_lock(&data->mutex);

    for (int i = 0; i < 5; i++) {
        mem[i] = data->mem_info[i];
    }

    for (int i = 0; i < 3; i++) {
        cpu[i] = data->cpu_info[i];
    }

    process_count = data->process_count;

    mem_ready = data->mem_ready;
    cpu_ready = data->cpu_ready;
    proc_ready = data->proc_ready;

    pthread_mutex_unlock(&data->mutex);

    // Print the local copies after releasing the mutex.
    printf("Memory data:\n");

    if (mem_ready) {
        const char* phrases[5] = {
            "MemTotal:",
            "MemFree:",
            "MemAvailable:",
            "SwapTotal:",
            "SwapFree:"
        };

        for (int i = 0; i < 5; i++) {
            printf("%s %d kB\n", phrases[i], mem[i]);
        }
    } else {
        printf("Waiting for memory sensor...\n");
    }

    printf("\nCPU load averages:\n");

    if (cpu_ready) {
        printf("1-minute load average: %.2f\n", cpu[0]);
        printf("5-minute load average: %.2f\n", cpu[1]);
        printf("15-minute load average: %.2f\n", cpu[2]);
    } else {
        printf("Waiting for CPU sensor...\n");
    }

    printf("\nProcesses data:\n");

    if (proc_ready) {
        printf("Total processes: %d\n", process_count);
    } else {
        printf("Waiting for process sensor...\n");
    }

    printf("\n------------------------\n");
    fflush(stdout);
}

int main(void) {
    SystemData data = {0};
    pthread_t threads[3];

    int rc = pthread_mutex_init(&data.mutex, NULL);

    if (rc != 0) {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(rc));
        return EXIT_FAILURE;
    }

    void* (*sensors[3])(void*) = {
        mem_sensor,
        cpu_sensor,
        proc_sensor
    };

    for (int i = 0; i < 3; i++) {
        rc = pthread_create(&threads[i], NULL, sensors[i], &data);

        if (rc != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    while (1) {
        get_data(&data);
        sleep(5);
    }

    return 0;
}
