#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

struct job {
    struct job* next;
    int id;
};

/* A linked list of pending jobs. */
struct job* job_queue = NULL;

/* A mutex protecting job_queue. */
pthread_mutex_t job_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Process a job. */
void process_job(struct job* job) {
    printf("Processing job %d (thread %lu)\n",
           job->id, (unsigned long)pthread_self());
}

/* Process queued jobs until the queue is empty. */
void* thread_function(void* arg) {
    while (1) {
        struct job* next_job;

        pthread_mutex_lock(&job_queue_mutex);

        if (job_queue == NULL)
            next_job = NULL;
        else {
            next_job = job_queue;
            job_queue = job_queue->next;
        }

        pthread_mutex_unlock(&job_queue_mutex);

        if (next_job == NULL)
            break;

        process_job(next_job);
        free(next_job);
    }

    return NULL;
}

int main() {
    pthread_t threads[3];

    /* Insert jobs into the queue. */
    for (int i = 0; i < 10; i++) {
        struct job* new_job = malloc(sizeof(struct job));

        if (new_job == NULL) {
            perror("malloc");
            return 1;
        }

        new_job->id = i + 1;
        new_job->next = NULL;

        /* Add job to the end of the queue. */
        pthread_mutex_lock(&job_queue_mutex);

        if (job_queue == NULL)
            job_queue = new_job;
        else {
            struct job* current = job_queue;

            while (current->next != NULL)
                current = current->next;

            current->next = new_job;
        }

        pthread_mutex_unlock(&job_queue_mutex);
    }

    /* Create worker threads. */
    for (int i = 0; i < 3; i++)
        pthread_create(&threads[i], NULL, thread_function, NULL);

    /* Wait for all threads to finish. */
    for (int i = 0; i < 3; i++)
        pthread_join(threads[i], NULL);

    return 0;
}
