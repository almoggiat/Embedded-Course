#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sysmon_ipc.h>

extern char **environ;

enum { MEM, CPU, PROC, SENSOR_COUNT };

static char *programs[SENSOR_COUNT] = {
    "./bin/mem_sensor",
    "./bin/cpu_sensor",
    "./bin/proc_sensor"
};

static const char *fifo_paths[SENSOR_COUNT] = {
    NULL, CPU_FIFO_PATH, PROC_FIFO_PATH
};

static int inputs[SENSOR_COUNT] = {-1, -1, -1};
static pid_t pids[SENSOR_COUNT];
static unsigned int restarts[SENSOR_COUNT];
static int started_before[SENSOR_COUNT];
static size_t received[SENSOR_COUNT];
static int fifo_created[SENSOR_COUNT];

static volatile sig_atomic_t stopping;

static void request_shutdown(int sig)
{
    (void)sig;
    stopping = 1;
}

static int make_proc(int id)
{
    int fds[2];
    int mem_write = -1;
    char path[64];
    char fd_arg[32];
    char *args[] = {programs[id], NULL, NULL};

    if (id == MEM) {
        if (pipe(fds) == -1)
            return -1;

        snprintf(path, sizeof(path), "/proc/self/fd/%d", fds[0]);
        inputs[MEM] = open(path, O_RDONLY | O_NONBLOCK);

        int open_error = errno;
        close(fds[0]);

        if (inputs[MEM] == -1) {
            close(fds[1]);
            errno = open_error;
            return -1;
        }

        mem_write = fds[1];
        snprintf(fd_arg, sizeof(fd_arg), "%d", mem_write);
        args[1] = fd_arg;
    }

    pid_t pid = fork();

    if (pid == 0) {
        for (int i = 0; i < SENSOR_COUNT; i++) {
            if (inputs[i] >= 0)
                close(inputs[i]);
        }

        execve(programs[id], args, environ);
        _exit(127);
    }

    int fork_error = errno;

    if (mem_write >= 0)
        close(mem_write);

    if (pid == -1) {
        if (id == MEM) {
            close(inputs[MEM]);
            inputs[MEM] = -1;
        }

        errno = fork_error;
        return -1;
    }

    pids[id] = pid;

    if (started_before[id]) {
        restarts[id]++;
        printf("%s respawned (PID %ld). Restart Count: %u\n",
               programs[id], (long)pid, restarts[id]);
        fflush(stdout);
    }

    started_before[id] = 1;
    return 0;
}

static void print_sensor(int id, const void *buffer)
{
    if (id == MEM) {
        const MemData *mem = buffer;

        printf("\n--- Memory Information ---\n");
        printf("Total memory:     %ld kB\n", mem->mem_total);
        printf("Free memory:      %ld kB\n", mem->mem_free);
        printf("Available memory: %ld kB\n", mem->mem_avail);
        printf("Total swap:       %ld kB\n", mem->swap_total);
        printf("Free swap:        %ld kB\n", mem->swap_free);
    } else if (id == CPU) {
        const CpuData *cpu = buffer;

        printf("\n--- CPU Information ---\n");
        printf("Load average (1 min):  %.2f\n", cpu->load1);
        printf("Load average (5 min):  %.2f\n", cpu->load5);
        printf("Load average (15 min): %.2f\n", cpu->load15);
    } else {
        const ProcData *proc = buffer;

        printf("\n--- Process Information ---\n");
        printf("Total processes: %d\n", proc->total_procs);
    }

    fflush(stdout);
}

static void check_sensor(int id, void *buffer, size_t size)
{
    if (pids[id] == 0) {
        if (make_proc(id) == -1)
            perror(programs[id]);

        return;
    }

    char *next = (char *)buffer + received[id];
    ssize_t n = read(inputs[id], next, size - received[id]);

    if (n > 0) {
        received[id] += (size_t)n;

        if (received[id] == size) {
            print_sensor(id, buffer);
            received[id] = 0;
        }

        return;
    }

    if (n == -1 && errno != EAGAIN &&
        errno != EWOULDBLOCK && errno != EINTR) {
        perror("read sensor");
        kill(pids[id], SIGKILL);
    }

    int status;
    pid_t result = waitpid(pids[id], &status, WNOHANG);

    if (result <= 0)
        return;

    if (WIFSIGNALED(status)) {
        printf("%s terminated by signal %d.\n",
               programs[id], WTERMSIG(status));
    } else if (WIFEXITED(status)) {
        printf("%s exited with status %d.\n",
               programs[id], WEXITSTATUS(status));
    }

    fflush(stdout);

    pids[id] = 0;
    received[id] = 0;

    if (id == MEM) {
        close(inputs[MEM]);
        inputs[MEM] = -1;
    }
}

static void cleanup(void)
{
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (pids[i] > 0)
            kill(pids[i], SIGKILL);

        if (inputs[i] >= 0)
            close(inputs[i]);
    }

    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (pids[i] > 0) {
            while (waitpid(pids[i], NULL, 0) == -1 && errno == EINTR) {
            }
        }

        if (fifo_created[i])
            unlink(fifo_paths[i]);
    }
}

int main(void)
{
    MemData mem;
    CpuData cpu;
    ProcData proc;

    void *buffers[] = {&mem, &cpu, &proc};
    size_t sizes[] = {sizeof(mem), sizeof(cpu), sizeof(proc)};

    if (signal(SIGINT, request_shutdown) == SIG_ERR ||
        signal(SIGTERM, request_shutdown) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    for (int i = CPU; i < SENSOR_COUNT; i++) {
        if (mkfifo(fifo_paths[i], 0666) == -1 && errno != EEXIST) {
            perror("mkfifo");
            cleanup();
            return 1;
        }

        fifo_created[i] = 1;
        inputs[i] = open(fifo_paths[i], O_RDONLY | O_NONBLOCK);

        if (inputs[i] == -1) {
            perror("open FIFO");
            cleanup();
            return 1;
        }
    }

    puts("System monitor started.");

    while (!stopping) {
        for (int i = 0; i < SENSOR_COUNT && !stopping; i++)
            check_sensor(i, buffers[i], sizes[i]);
    }

    cleanup();
    return 0;
}
