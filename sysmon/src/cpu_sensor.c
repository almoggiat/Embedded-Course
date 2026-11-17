#include <stdio.h>
#include <unistd.h>
#include "sysmond.h"

void* cpu_sensor(void* arg) {
    SystemData* data = arg;

    while (1) {
        FILE* file = fopen("/proc/loadavg", "r");

        if (file == NULL) {
            perror("fopen loadavg");
            sleep(5);
            continue;
        }

        double values[3];

        int count = fscanf(file, "%lf %lf %lf",
                           &values[0],
                           &values[1],
                           &values[2]);

        fclose(file);

        if (count == 3) {
            pthread_mutex_lock(&data->mutex);

            for (int i = 0; i < 3; i++) {
                data->cpu_info[i] = values[i];
            }

            data->cpu_ready = true;

            pthread_mutex_unlock(&data->mutex);
        } else {
            fprintf(stderr, "Could not read CPU data\n");
        }

        sleep(5);
    }

    return NULL;
}
