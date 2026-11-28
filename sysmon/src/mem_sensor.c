
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <mqueue.h>

#include <sysmon_ipc.h>


int read_mem(mqd_t queue) {
    FILE *mem_file = fopen("/proc/meminfo", "r");

    if (mem_file == NULL) {
        perror("fopen meminfo");
        return 1;
    }

    MemData data = {0};

    char line[512];

    while (fgets(line, sizeof(line), mem_file) != NULL) {

        if (sscanf(line, "MemTotal: %ld", &data.mem_total) == 1)
            continue;

        if (sscanf(line, "MemFree: %ld", &data.mem_free) == 1)
            continue;

        if (sscanf(line, "MemAvailable: %ld", &data.mem_avail) == 1)
            continue;

        if (sscanf(line, "SwapTotal: %ld", &data.swap_total) == 1)
            continue;

        if (sscanf(line, "SwapFree: %ld", &data.swap_free) == 1)
            continue;
    }

    fclose(mem_file);

    if (mq_send(queue, (const char *)&data, sizeof(data), 0) == -1) {
        perror("mq_send");
        return 1;
    }

    return 0;
}


int main(void) {

    mqd_t queue = mq_open(MEM_QUEUE_NAME, O_WRONLY);

    if (queue == (mqd_t)-1) {
        perror("mq_open MEM_QUEUE_NAME");
        return 1;
    }

    while (1) {

        if (read_mem(queue) != 0) {
            mq_close(queue);
            return 1;
        }

        sleep(5);
    }

    mq_close(queue);
    return 0;
}
