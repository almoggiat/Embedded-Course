
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#include <sysmon_ipc.h>


#define SENSOR_COUNT 3

#define MEM_SENSOR  0
#define CPU_SENSOR  1
#define PROC_SENSOR 2


int fds[2] = {-1, -1};

int cpu_fd = -1;
int proc_fd = -1;

pid_t sensor_pids[SENSOR_COUNT] = {-1, -1, -1};

int created_cpu_fifo = 0;
int created_proc_fifo = 0;

volatile sig_atomic_t stopping = 0;

extern char **environ;


/* ---------- Signal handling ---------- */

void handle_signal(int sig) {
    (void)sig;
    stopping = 1;
}


/* ---------- IPC setup ---------- */

int make_pipe(void) {
    if (pipe(fds) == -1) {
        perror("pipe");
        return 1;
    }

    return 0;
}


int make_fifo(const char *path, int *created) {
    struct stat st;

    if (mkfifo(path, 0666) == 0) {
        *created = 1;
        return 0;
    }

    if (errno != EEXIST) {
        perror("mkfifo");
        return 1;
    }

    /* Verify that the existing path is actually a FIFO. */
    if (lstat(path, &st) == -1) {
        perror("lstat FIFO");
        return 1;
    }

    if (!S_ISFIFO(st.st_mode)) {
        fprintf(stderr, "%s exists but is not a FIFO\n", path);
        return 1;
    }

    return 0;
}


int make_dir(void) {
    struct stat st;

    if (mkdir("/tmp/sysmon", 0755) == -1) {
        if (errno != EEXIST) {
            perror("mkdir");
            return 1;
        }

        if (stat("/tmp/sysmon", &st) == -1 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "/tmp/sysmon exists but is not a directory\n");
            return 1;
        }
    }

    return 0;
}


/* ---------- Data reception ---------- */

/*
 * Return values:
 *  1 = complete structure received
 *  0 = EOF / sensor disconnected
 * -1 = communication error
 */

int receive_data(int fd, void *buffer, size_t size) {
    char *ptr = (char *)buffer;
    size_t received = 0;

    while (received < size) {
        ssize_t n = read(fd, ptr + received, size - received);

        if (n < 0) {
            if (errno == EINTR) {
                if (stopping)
                    return 0;

                continue;
            }

            perror("read");
            return -1;
        }

        if (n == 0) {
            if (received == 0)
                return 0;

            fprintf(stderr,
                    "Incomplete data received (%zu/%zu bytes)\n",
                    received, size);

            return -1;
        }

        received += n;
    }

    return 1;
}


/* ---------- Memory sensor ---------- */

int get_mem_data(int fd) {
    MemData data;

    int result = receive_data(fd, &data, sizeof(data));

    if (result != 1)
        return result;

    printf("\n--- Memory Information ---\n");
    printf("Total memory:     %ld kB\n", data.mem_total);
    printf("Free memory:      %ld kB\n", data.mem_free);
    printf("Available memory: %ld kB\n", data.mem_avail);
    printf("Total swap:       %ld kB\n", data.swap_total);
    printf("Free swap:        %ld kB\n", data.swap_free);

    return 1;
}


/* ---------- CPU sensor ---------- */

int get_cpu_data(int fd) {
    CpuData data;

    int result = receive_data(fd, &data, sizeof(data));

    if (result != 1)
        return result;

    printf("\n--- CPU Information ---\n");
    printf("Load average (1 min):  %.2f\n", data.load1);
    printf("Load average (5 min):  %.2f\n", data.load5);
    printf("Load average (15 min): %.2f\n", data.load15);

    return 1;
}


/* ---------- Process sensor ---------- */

int get_proc_data(int fd) {
    ProcData data;

    int result = receive_data(fd, &data, sizeof(data));

    if (result != 1)
        return result;

    printf("\n--- Process Information ---\n");
    printf("Total processes: %d\n", data.total_procs);

    return 1;
}


/* ---------- Combined data reception ---------- */

int get_data(void) {
    int result;

    result = get_mem_data(fds[0]);
    if (result != 1)
        return result;

    result = get_cpu_data(cpu_fd);
    if (result != 1)
        return result;

    result = get_proc_data(proc_fd);
    if (result != 1)
        return result;

    return 1;
}


/* ---------- Sensor process creation ---------- */

int make_proc(int num) {
    char *path;

    if (num == MEM_SENSOR)
        path = "./bin/mem_sensor";
    else if (num == CPU_SENSOR)
        path = "./bin/cpu_sensor";
    else if (num == PROC_SENSOR)
        path = "./bin/proc_sensor";
    else
        return 1;


    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }


    if (pid == 0) {
        char fd_str[16];
        char *args[3] = {path, NULL, NULL};

        if (num == MEM_SENSOR) {
            /* Memory sensor uses the anonymous pipe. */
            close(fds[0]);

            snprintf(fd_str, sizeof(fd_str), "%d", fds[1]);
            args[1] = fd_str;
        }
        else {
            /* CPU and process sensors use named FIFOs. */
            close(fds[0]);
            close(fds[1]);
        }

        execve(path, args, environ);

        perror("execve");
        _exit(1);
    }


    sensor_pids[num] = pid;

    /* Parent does not need the pipe's write end. */
    if (num == MEM_SENSOR) {
        close(fds[1]);
        fds[1] = -1;
    }

    return 0;
}


/* ---------- Cleanup ---------- */

void cleanup(void) {
    /* Close communication descriptors. */

    if (fds[0] != -1) {
        close(fds[0]);
        fds[0] = -1;
    }

    if (fds[1] != -1) {
        close(fds[1]);
        fds[1] = -1;
    }

    if (cpu_fd != -1) {
        close(cpu_fd);
        cpu_fd = -1;
    }

    if (proc_fd != -1) {
        close(proc_fd);
        proc_fd = -1;
    }


    /* Terminate remaining sensor processes. */

    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (sensor_pids[i] > 0)
            kill(sensor_pids[i], SIGTERM);
    }


    /* Reap child processes. */

    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (sensor_pids[i] > 0) {
            while (waitpid(sensor_pids[i], NULL, 0) == -1) {
                if (errno == EINTR)
                    continue;

                break;
            }

            sensor_pids[i] = -1;
        }
    }


    /* Remove FIFOs created by this instance. */

    if (created_cpu_fifo)
        unlink(CPU_FIFO_PATH);

    if (created_proc_fifo)
        unlink(PROC_FIFO_PATH);
}


/* ---------- Main ---------- */

int main(void) {

    /* Create the directory. */

    if (make_dir() != 0)
        return 1;


    /* Create the named FIFOs. */

    if (make_fifo(CPU_FIFO_PATH, &created_cpu_fifo) != 0) {
        cleanup();
        return 1;
    }

    if (make_fifo(PROC_FIFO_PATH, &created_proc_fifo) != 0) {
        cleanup();
        return 1;
    }


    /* Create the anonymous pipe. */

    if (make_pipe() != 0) {
        cleanup();
        return 1;
    }


    /* Launch all three sensors. */

    for (int k = 0; k < SENSOR_COUNT; k++) {
        if (make_proc(k) != 0) {
            fprintf(stderr, "Failed to create sensor %d\n", k);
            cleanup();
            return 1;
        }
    }


    /* Install signal handlers after forking. */

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);


    /*
     * Open the FIFOs for reading.
     * These calls block until their corresponding sensors
     * open the FIFOs for writing.
     */

    cpu_fd = open(CPU_FIFO_PATH, O_RDONLY);

    if (cpu_fd == -1) {
        if (!stopping)
            perror("open CPU FIFO");

        cleanup();
        return 1;
    }


    proc_fd = open(PROC_FIFO_PATH, O_RDONLY);

    if (proc_fd == -1) {
        if (!stopping)
            perror("open process FIFO");

        cleanup();
        return 1;
    }


    printf("System monitor started.\n");


    /* Receive data continuously. */

    while (!stopping) {
        int result = get_data();

        if (result == 0) {
            if (!stopping)
                fprintf(stderr, "A sensor disconnected\n");

            break;
        }

        if (result == -1) {
            fprintf(stderr, "Sensor communication error\n");
            break;
        }

        fflush(stdout);
    }


    printf("\nStopping system monitor...\n");

    cleanup();

    printf("System monitor stopped.\n");

    return 0;
}
