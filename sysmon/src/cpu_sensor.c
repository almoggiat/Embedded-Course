
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sysmon_ipc.h>


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


int send_cpu_data(int fd, CpuData *data) {
    const char *ptr = (const char *)data;
    size_t remaining = sizeof(*data);

    while (remaining > 0) {
        ssize_t written = write(fd, ptr, remaining);

        if (written < 0) {
            if (errno == EINTR)
                continue;

            perror("write CPU FIFO");
            return 1;
        }

        if (written == 0) {
            fprintf(stderr, "FIFO write returned 0\n");
            return 1;
        }

        ptr += written;
        remaining -= written;
    }

    return 0;
}


int main(void) {
    int fifo_fd = open(CPU_FIFO_PATH, O_WRONLY);

    if (fifo_fd == -1) {
        perror("open CPU FIFO");
        return 1;
    }

    while (1) {
        CpuData data;

        if (read_cpu(&data) != 0) {
            close(fifo_fd);
            return 1;
        }

        if (send_cpu_data(fifo_fd, &data) != 0) {
            close(fifo_fd);
            return 1;
        }

        sleep(5);
    }

    close(fifo_fd);
    return 0;
}
