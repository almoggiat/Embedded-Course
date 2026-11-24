#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sysmon_ipc.h>

extern char **environ;

int fds[2];
int inputs[3] = {-1, -1, -1};
int ids[3] = {0, 1, 2};
pid_t pids[3] = {0};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int receive_data(int fd, void* buffer, size_t size) {
    char* ptr = buffer;

    while (size > 0) {
        ssize_t n = read(fd, ptr, size);

        if (n == -1 && errno == EINTR)
            continue;

        if (n <= 0)
            return 0;

        ptr += n;
        size -= n;
    }

    return 1;
}

void* read_sensor(void* arg) {
    int id = *(int*)arg;
    MemData mem;
    CpuData cpu;
    ProcData proc;

    void* buffers[] = {&mem, &cpu, &proc};
    size_t sizes[] = {sizeof(mem), sizeof(cpu), sizeof(proc)};

    while (receive_data(inputs[id], buffers[id], sizes[id])) {
        pthread_mutex_lock(&mutex);

        if (id == 0) {
            printf("\n--- Memory Information ---\n");
            printf("Total memory:     %ld kB\n", mem.mem_total);
            printf("Free memory:      %ld kB\n", mem.mem_free);
            printf("Available memory: %ld kB\n", mem.mem_avail);
            printf("Total swap:       %ld kB\n", mem.swap_total);
            printf("Free swap:        %ld kB\n", mem.swap_free);
        } else if (id == 1) {
            printf("\n--- CPU Information ---\n");
            printf("Load average (1 min):  %.2f\n", cpu.load1);
            printf("Load average (5 min):  %.2f\n", cpu.load5);
            printf("Load average (15 min): %.2f\n", cpu.load15);
        } else {
            printf("\n--- Process Information ---\n");
            printf("Total processes: %d\n", proc.total_procs);
        }

        fflush(stdout);
        pthread_mutex_unlock(&mutex);
    }

    fprintf(stderr, "Sensor %d disconnected or read failed\n", id);
    return NULL;
}

int make_proc(int num) {
    char* paths[] = {
        "./bin/mem_sensor",
        "./bin/cpu_sensor",
        "./bin/proc_sensor"
    };

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        char fd_str[32];
        char* args[] = {paths[num], NULL, NULL};

        close(fds[0]);

        if (num == 0) {
            snprintf(fd_str, sizeof(fd_str), "%d", fds[1]);
            args[1] = fd_str;
        } else {
            close(fds[1]);
        }

        execve(paths[num], args, environ);
        perror("execve");
        _exit(1);
    }

    pids[num] = pid;
    return 0;
}

void cleanup(void) {
    for (int i = 0; i < 3; i++) {
        if (pids[i] > 0)
            kill(pids[i], SIGTERM);
    }

    for (int i = 0; i < 3; i++) {
        if (pids[i] > 0) {
            while (waitpid(pids[i], NULL, 0) == -1 &&
                   errno == EINTR) {
            }
        }
    }
}

int main(void) {
    const char* paths[] = {CPU_FIFO_PATH, PROC_FIFO_PATH};

    for (int i = 0; i < 2; i++) {
        if (mkfifo(paths[i], 0666) == -1 && errno != EEXIST) {
            perror("mkfifo");
            return 1;
        }
    }

    if (pipe(fds) == -1) {
        perror("pipe");
        return 1;
    }

    for (int i = 0; i < 3; i++) {
        if (make_proc(i) != 0) {
            cleanup();
            return 1;
        }
    }

    close(fds[1]);
    inputs[0] = fds[0];

    for (int i = 0; i < 2; i++) {
        inputs[i + 1] = open(paths[i], O_RDONLY);

        if (inputs[i + 1] == -1) {
            perror("open FIFO");
            cleanup();
            return 1;
        }
    }

    pthread_t threads[3];

    printf("System monitor started.\n");

    for (int i = 0; i < 3; i++) {
        int result = pthread_create(
            &threads[i], NULL, read_sensor, &ids[i]
        );

        if (result != 0) {
            fprintf(stderr, "pthread_create failed: %d\n", result);
            cleanup();
            return 1;
        }
    }

    for (int i = 0; i < 3; i++)
        pthread_join(threads[i], NULL);

    cleanup();

    for (int i = 0; i < 3; i++)
        close(inputs[i]);

    pthread_mutex_destroy(&mutex);
    return 0;
}
