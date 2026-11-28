
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <mqueue.h>
#include "sysmon_ipc.h"


int read_cpu(CpuData *data) {
    FILE *cpu_file = fopen("/proc/loadavg", "r");

    if (cpu_file == NULL) {
        perror("fopen loadavg");
        return 1;
    }

    int result = fscanf(cpu_file, "%lf %lf %lf",
                        &data->load1,
                        &data->load5,
                        &data->load15);

    fclose(cpu_file);

    if (result != 3) {
        fprintf(stderr, "Could not read all three load averages\n");
        return 1;
    }

    return 0;
}


int send_cpu_data(mqd_t queue, CpuData *data) {

    if (mq_send(queue, (const char *)data, sizeof(*data), 0) == -1) {
        perror("mq_send CPU");
        return 1;
    }

    return 0;
}


int main(void) {

    mqd_t queue = mq_open(CPU_QUEUE_NAME, O_WRONLY);
    if (queue == (mqd_t)-1) {
        perror("mq_open CPU_QUEUE_NAME");
        return 1;
    }

    if (sizeof(CpuData) > 512) {
        fprintf(stderr, "CpuData exceeds message queue size\n");
        mq_close(queue);
        return 1;
    }

    while (1) {
        CpuData data;

        if (read_cpu(&data) != 0) {
            mq_close(queue);
            return 1;
        }

        if (send_cpu_data(queue, &data) != 0) {
	  
            mq_close(queue);
            return 1;
        }

        sleep(11);
    }

    mq_close(queue);
    return 0;
}
