
#include "queue.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

struct queue { // FIFO

    int max_size;

    int in; // next empty index where we can push something
    int out; // index of the oldest added item

    sem_t full_cells; // number of full cells-- called "empty" in slides
    sem_t empty_cells; // number of empty cells left-- called "full" in slides

    pthread_mutex_t mutex; // lock to protect buffer data

    void **array;
};

// constructor
queue_t *queue_new(int size) {

    queue_t *q = calloc(1, sizeof(queue_t)); // allocate memory for entire struct

    q->array = calloc(size, sizeof(void *)); // allocate memory for the actual array

    // initialize fields!
    q->max_size = size;
    q->in = 0;
    q->out = 0;

    // initialize this semaphore, with default flags (0), to start at 0
    sem_init(&q->full_cells, 0, 0);
    // initialize this semaphore, with default flags (0), to start at size
    sem_init(&q->empty_cells, 0, size);

    // initialize          this mutex      with default attributes
    pthread_mutex_init(&q->mutex, NULL);

    return q;
}

// destructor
void queue_delete(queue_t **q) {

    // destroy mutexes and semaphores
    pthread_mutex_destroy(&(*q)->mutex);
    sem_destroy(&(*q)->full_cells);
    sem_destroy(&(*q)->empty_cells);

    // free array
    free((*q)->array);
    (*q)->array = NULL;

    // free overall struct memory
    free(*q);
    *q = NULL;
}

// add an item to the queue
bool queue_push(queue_t *q, void *elem) {

    if (q == NULL)
        return false;

    sem_wait(&q->empty_cells);

    pthread_mutex_lock(&q->mutex);

    q->array[q->in] = elem;
    q->in = (q->in + 1) % q->max_size;

    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->full_cells);

    return true;
}

bool queue_pop(queue_t *q, void **elem) {

    if (q == NULL)
        return false;

    sem_wait(&q->full_cells);
    pthread_mutex_lock(&q->mutex);

    *elem = q->array[q->out];
    q->out = (q->out + 1) % q->max_size;

    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->empty_cells);

    return true;
}
