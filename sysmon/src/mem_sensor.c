
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>

#include <sysmon_ipc.h>


int read_mem(int pipe_fd) {
    FILE *mem_file = fopen("/proc/meminfo", "r");

    if (mem_file == NULL) {
        perror("fopen meminfo");
        return 1;
    }

    MemData data = {0};

    char line[256];

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

    // Send the entire structure through the pipe.
    const char *ptr = (const char *)&data;
    size_t remaining = sizeof(data);

    while (remaining > 0) {
        ssize_t written = write(pipe_fd, ptr, remaining);

        if (written < 0) {
            if (errno == EINTR)
                continue;

            perror("write pipe");
            return 1;
        }

        if (written == 0) {
            fprintf(stderr, "Pipe write returned 0\n");
            return 1;
        }

        ptr += written;
        remaining -= written;
    }

    return 0;
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Missing pipe descriptor\n");
        return 1;
    }

    int pipe_fd = atoi(argv[1]);

    while (1) {
        if (read_mem(pipe_fd) != 0) {
            close(pipe_fd);
            return 1;
        }

        sleep(5);
    }

    close(pipe_fd);
    return 0;
}
