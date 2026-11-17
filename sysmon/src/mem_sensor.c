#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sysmond.h"

int get_int_from_str(const char* string) {
    for (size_t i = 0; string[i] != '\0'; i++) {
        if (string[i] >= '0' && string[i] <= '9') {
            return atoi(string + i);
        }
    }

    return 0;
}

void* mem_sensor(void* arg) {
    SystemData* data = arg;

    // Verify these rows on your machine.
    int line_numbers[5] = {1, 2, 3, 15, 16};

    while (1) {
        FILE* file = fopen("/proc/meminfo", "r");

        if (file == NULL) {
            perror("fopen meminfo");
            sleep(5);
            continue;
        }

        int values[5] = {0};
        char line[256];
        int counter = 0;

        for (int row = 1;
             counter < 5 && fgets(line, sizeof(line), file) != NULL;
             row++) {

            if (row == line_numbers[counter]) {
                values[counter] = get_int_from_str(line);
                counter++;
            }
        }

        int read_failed = ferror(file);
        fclose(file);

        if (counter == 5 && !read_failed) {
            pthread_mutex_lock(&data->mutex);

            for (int i = 0; i < 5; i++) {
                data->mem_info[i] = values[i];
            }

            data->mem_ready = true;

            pthread_mutex_unlock(&data->mutex);
        } else {
            fprintf(stderr, "Could not read memory data\n");
        }

        sleep(5);
    }

    return NULL;
}
