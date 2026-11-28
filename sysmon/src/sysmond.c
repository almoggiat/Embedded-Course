
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sysmon_ipc.h>
#include <stdlib.h>
#include <string.h>

extern char **environ;

enum { MEM, CPU, PROC, SENSOR_COUNT };

static char *programs[SENSOR_COUNT] = {
    "./bin/mem_sensor",
    "./bin/cpu_sensor",
    "./bin/proc_sensor"
};

static const char *queue_names[SENSOR_COUNT] = {
    MEM_QUEUE_NAME,
    CPU_QUEUE_NAME,
    PROC_QUEUE_NAME
};

static mqd_t queues[SENSOR_COUNT] = {
    (mqd_t)-1, (mqd_t)-1, (mqd_t)-1
};

static pid_t pids[SENSOR_COUNT];
static unsigned int restarts[SENSOR_COUNT];
static int started_before[SENSOR_COUNT];

static volatile sig_atomic_t stopping = 0;


static void request_shutdown(int sig)
{
    (void)sig;
    stopping = 1;
}


static int make_proc(int id)
{
    char *args[] = {programs[id], NULL};
    pid_t pid = fork();

    if (pid == -1)
        return -1;

    if (pid == 0) {
        execve(programs[id], args, environ);
        perror(programs[id]);
	exit(1);
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


static void check_sensor(int id)
{
    int status;
    pid_t result;

    if (pids[id] > 0) {
        result = waitpid(pids[id], &status, WNOHANG);

        if (result == 0)
            return;

        if (result == -1) {
            if (errno != EINTR)
                perror("waitpid sensor");
            return;
        }

        if (WIFSIGNALED(status)) {
            printf("%s terminated by signal %d.\n",
                   programs[id], WTERMSIG(status));
        } else if (WIFEXITED(status)) {
            printf("%s exited with status %d.\n",
                   programs[id], WEXITSTATUS(status));
        }

        fflush(stdout);
        pids[id] = 0;
    }

    if (!stopping && make_proc(id) == -1)
        perror(programs[id]);
}


static void cleanup(void)
{
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (pids[i] > 0)
            kill(pids[i], SIGKILL);
    }

    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (pids[i] > 0) {
            while (waitpid(pids[i], NULL, 0) == -1 &&
                   errno == EINTR) {
            }
        }
    }

    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (queues[i] != (mqd_t)-1) {
            mq_close(queues[i]);
            mq_unlink(queue_names[i]);
            queues[i] = (mqd_t)-1;
        }
    }
}


static const struct mq_attr attributes = {
    .mq_flags = 0,
    .mq_maxmsg = 1,
    .mq_msgsize = 256,
    .mq_curmsgs = 0
};


static int init_queues(void)
{
    for (int i = 0; i < SENSOR_COUNT; i++) {

        queues[i] = mq_open(
            queue_names[i],
            O_RDONLY | O_CREAT | O_NONBLOCK,
            0666,
            &attributes
        );

        if (queues[i] == (mqd_t)-1) {
            perror("mq_open");
            return -1;
        }
    }

    return 0;
}


static void receive_messages(void)
{
    char buffer[512];
    unsigned int priority;

    for (int i = 0; i < SENSOR_COUNT; i++) {

        while (1) {
            ssize_t bytes = mq_receive(
                queues[i],
                buffer,
                sizeof(buffer),
                &priority
            );

            if (bytes == -1) {

                if (errno == EAGAIN)
                    break;

                if (errno == EINTR)
                    continue;

                perror("mq_receive");
                break;
            }

            printf("[%s] ", programs[i]);

            switch (i) {

                case MEM: {
                    MemData data;

                    if (bytes != sizeof(data)) {
                        printf("Invalid MemData size\n");
                        break;
                    }

                    memcpy(&data, buffer, sizeof(data));

                    printf("MemTotal: %ld kB | "
                           "MemFree: %ld kB | "
                           "MemAvailable: %ld kB | "
                           "SwapTotal: %ld kB | "
                           "SwapFree: %ld kB\n",
                           data.mem_total,
                           data.mem_free,
                           data.mem_avail,
                           data.swap_total,
                           data.swap_free);
                    break;
                }

                case CPU: {
                    CpuData data;

                    if (bytes != sizeof(data)) {
                        printf("Invalid CpuData size\n");
                        break;
                    }

                    memcpy(&data, buffer, sizeof(data));

                    printf("Load average: %.2f, %.2f, %.2f\n",
                           data.load1,
                           data.load5,
                           data.load15);
                    break;
                }

                case PROC: {
                    ProcData data;

                    if (bytes != sizeof(data)) {
                        printf("Invalid ProcData size\n");
                        break;
                    }

                    memcpy(&data, buffer, sizeof(data));

                    printf("Total processes: %d\n",
                           data.total_procs);
                    break;
                }
            }
        }
    }
}



int main(void)
{
    if (signal(SIGINT, request_shutdown) == SIG_ERR ||
        signal(SIGTERM, request_shutdown) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    if (init_queues() == -1) {
        cleanup();
        return 1;
    }

    puts("System monitor started.");

    while (!stopping) {

        for (int i = 0; i < SENSOR_COUNT && !stopping; i++)
            check_sensor(i);

        receive_messages();

        if (!stopping)
            sleep(1);
    }

    cleanup();

    puts("System monitor stopped.");

    return 0;
}
