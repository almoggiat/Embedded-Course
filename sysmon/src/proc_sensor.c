#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <mqueue.h>
#include "sysmon_ipc.h"


int is_digits(const char *str) {
    if (str[0] == '\0')
        return 0;

    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9')
            return 0;
    }

    return 1;
}

int read_proc(ProcData *data) {
    DIR *dir = opendir("/proc");

    if (dir == NULL) {
        perror("opendir /proc");
        return 1;
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (is_digits(entry->d_name))
            count++;
    }

    closedir(dir);

    data->total_procs = count;

    return 0;
}


int send_proc_data(mqd_t queue, ProcData *data) {

    if (mq_send(queue, (const char *)data, sizeof(*data), 0) == -1) {
        perror("mq_send PROC");
        return 1;
    }

    return 0;
}


int main(void) {

    mqd_t queue = mq_open(PROC_QUEUE_NAME, O_WRONLY);

    if (queue == (mqd_t)-1) {
        perror("mq_open PROC_QUEUE_NAME");
        return 1;
    }

    if (sizeof(ProcData) > 512) {
        fprintf(stderr, "ProcData exceeds message queue size\n");
        mq_close(queue);
        return 1;
    }

    while (1) {
        ProcData data;

        if (read_proc(&data) != 0) {
            mq_close(queue);
            return 1;
        }

        if (send_proc_data(queue, &data) != 0) {
            mq_close(queue);
            return 1;
        }

        sleep(17);
    }

    mq_close(queue);
    return 0;
}
