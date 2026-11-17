#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include "sysmond.h"

void* proc_sensor(void* arg) {
    SystemData* data = arg;

    while (1) {
        DIR* dir = opendir("/proc");

        if (dir == NULL) {
            perror("opendir /proc");
            sleep(5);
            continue;
        }

        int count = 0;
        struct dirent* entry;

        while ((entry = readdir(dir)) != NULL) {
            bool numeric = entry->d_name[0] != '\0';

            for (int i = 0; entry->d_name[i] != '\0'; i++) {
                if (entry->d_name[i] < '0' ||
                    entry->d_name[i] > '9') {
                    numeric = false;
                    break;
                }
            }

            if (numeric) {
                count++;
            }
        }

        closedir(dir);

        pthread_mutex_lock(&data->mutex);

        data->process_count = count;
        data->proc_ready = true;

        pthread_mutex_unlock(&data->mutex);

        sleep(5);
    }

    return NULL;
}
