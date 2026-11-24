
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sysmon_ipc.h>


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


int send_proc_data(int fd, ProcData *data) {
    const char *ptr = (const char *)data;
    size_t remaining = sizeof(*data);

    while (remaining > 0) {
        ssize_t written = write(fd, ptr, remaining);

        if (written < 0) {
            if (errno == EINTR)
                continue;

            perror("write PROC FIFO");
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
    int fifo_fd = open(PROC_FIFO_PATH, O_WRONLY);

    if (fifo_fd == -1) {
        perror("open PROC FIFO");
        return 1;
    }

    while (1) {
        ProcData data;

        if (read_proc(&data) != 0) {
            close(fifo_fd);
            return 1;
        }

        if (send_proc_data(fifo_fd, &data) != 0) {
            close(fifo_fd);
            return 1;
        }

        sleep(17);
    }

    close(fifo_fd);
    return 0;
}
