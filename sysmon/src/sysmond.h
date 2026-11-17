#ifndef SYSMON_H
#define SYSMON_H

#include <pthread.h>
#include <stdbool.h>

typedef struct {
    int mem_info[5];
    double cpu_info[3];
    int process_count;

    bool mem_ready;
    bool cpu_ready;
    bool proc_ready;

    pthread_mutex_t mutex;
} SystemData;

void* mem_sensor(void* arg);
void* cpu_sensor(void* arg);
void* proc_sensor(void* arg);

#endif
