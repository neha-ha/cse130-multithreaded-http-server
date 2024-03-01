
#include "rwlock.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

struct rwlock {

    PRIORITY priority; // READER, WRITER, N-WAY, aka 0, 1, 2
    uint32_t n; // if n way priority.. what is n?

    uint32_t
        readers_in_between; // the number of reader threads that have gone since the last writer

    uint32_t active_readers; // number of currently active readers
    uint32_t active_writers; // number of currently active writers

    uint32_t waiting_readers; // number of currently waiting readers
    uint32_t waiting_writers; // number of currently waiting writers

    pthread_cond_t
        readers_go; // condition variable-- used to tell reader threads whether they can go or not
    pthread_cond_t
        writers_go; // condition variable-- used to tell writer threads whether they can go or not

    pthread_mutex_t
        mutex; // lock used to protect the variables "readers" and "writers" defined above
};

// helper function-- contains logic for which thread to signal next
// we don't acquire the mutex in here, since this function will only ever be called when the mutex is already acquired
// NOTE: notice we often check that all readers are finished before starting a write (active_readers == 0).
// we don't check the converse-- whether all writers are finished reading-- because only one writer can be reading at a time.
// if it is the one that just unlocked, obv no writers
// if a reader just unlocked, reads and writes cannot go on at the same time, so obv no active writers there too
void signal_next(rwlock_t *rw) {

    if (rw->priority == READERS) {
        // if the priority is READERS, and there are more readers waiting around, let all of them go!!
        if (rw->waiting_readers > 0) {
            pthread_cond_broadcast(&rw->readers_go);
        }
        // if no more readers waiting around and no readers reading right now, we can let one writer go i guess..
        else if (rw->active_readers == 0) {
            pthread_cond_signal(&rw->writers_go);
        }
    } else if (rw->priority == N_WAY) {
        // firstly, are there even any writers waiting around??
        if (rw->waiting_writers > 0) {
            // if there are, and enough readers have gone & ARE FINISHED, let a poor writer go
            if (rw->readers_in_between >= rw->n) {
                if (rw->active_readers == 0) {
                    pthread_cond_signal(&rw->writers_go);
                }
            }
            // not enough readers have gone... but are there even any readers around??
            else if (rw->waiting_readers > 0) {
                // if yes, let one go
                pthread_cond_signal(&rw->readers_go); // was signal before
            } else if (
                rw->active_readers
                == 0) { // not enough readers have gone in between, but there are no waiting readers. and all current readers have finished! and there is a writer waiting! let it go.
                pthread_cond_signal(&rw->writers_go);
            }
        } else { // no writers waiting... it doesn't matter if we have exceeded N readers between writers, just let all the readers waiting go
            pthread_cond_broadcast(&rw->readers_go);
        }
    } else { // priority == WRITERS
        // if there are no waiting writers around, let all the readers go
        if (rw->waiting_writers == 0) {
            pthread_cond_broadcast(&rw->readers_go);
        }
        // if there are writers, and all active readers have finished, let the next writer go
        else if (rw->active_readers == 0) {
            pthread_cond_signal(&rw->writers_go);
        }
    }
}

// constructor
rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {

    rwlock_t *rw = calloc(1, sizeof(rwlock_t)); // allocate memory!

    // initialize fields!
    rw->n = n;
    rw->priority = p;

    rw->active_readers = 0;
    rw->active_writers = 0;
    rw->waiting_readers = 0;
    rw->waiting_writers = 0;
    rw->readers_in_between = 0;

    // initialize          this cv         with default attributes
    pthread_cond_init(&rw->readers_go, NULL);
    pthread_cond_init(&rw->writers_go, NULL);

    // initialize          this mutex      with default attributes
    pthread_mutex_init(&rw->mutex, NULL);

    return rw;
}

// destructor
void rwlock_delete(rwlock_t **rw) {

    // destroy mutexes and cvs
    pthread_mutex_destroy(&(*rw)->mutex);
    pthread_cond_destroy(&(*rw)->readers_go);
    pthread_cond_destroy(&(*rw)->writers_go);

    // free overall memory
    free(*rw);
    *rw = NULL;
}

// a thread wants to read! it needs to acquire the reader lock... by calling this function
void reader_lock(rwlock_t *rw) {

    // we need to see if the reader is even allowed to read rn-- to do this, we need to access the "readers" and "writers" fields of rw
    // these fields are technically shared memory... so we need to grab the lock before looking at them
    pthread_mutex_lock(&rw->mutex);

    rw->waiting_readers++; // this reader thread is waiting rn

    fprintf(stderr, "waiting writers: %d\n", rw->waiting_writers);
    fprintf(stderr, "active writers: %d\n", rw->active_writers);
    fprintf(stderr, "waiting readers: %d\n", rw->waiting_readers);
    fprintf(stderr, "active readers: %d\n", rw->active_readers);

    fprintf(stderr, "readers in between: %d\n", rw->readers_in_between);

    // NOTE: don't read from socket after \r\n\r\n in asgn2, all tests should finish within 1 minute

    // combining the conditions-- when do we need to wait?
    while (rw->active_writers > 0 || (rw->priority == WRITERS && rw->waiting_writers > 0)
           || (rw->priority == N_WAY
               && (rw->waiting_writers > 0 && rw->readers_in_between >= rw->n))) {

        pthread_cond_wait(&rw->readers_go,
            &rw->mutex); // release the lock "mutex" & pause thread's execution. when woken, check for condition, re acquire when condition is met
    }

    rw->waiting_readers--; // this thread stopped waiting,
    rw->active_readers++; // and now it's reading

    rw->readers_in_between++; // this reader has gone since the last writer

    // now that we incremented / decremented, we don't acc need the lock on anymore
    pthread_mutex_unlock(&rw->mutex);

    //fprintf(stderr, "r locked\n");
}

// a thread is finished reading! itll let go of it's lock and signal other threads... using this function:
void reader_unlock(rwlock_t *rw) {

    // gonna mess w struct fields --> get a lock
    pthread_mutex_lock(&rw->mutex);

    rw->active_readers--; // this thread has finished reading

    // signal the next threads
    signal_next(rw);

    pthread_mutex_unlock(&rw->mutex); // release lock
}

// a thread wants to write! it needs to acquire the writer lock by calling this function
void writer_lock(rwlock_t *rw) {

    // gonna mess w struct fields --> get a lock
    pthread_mutex_lock(&rw->mutex);

    rw->waiting_writers++; // this thread is waiting to write

    fprintf(stderr, "waiting writers: %d\n", rw->waiting_writers);
    fprintf(stderr, "active writers: %d\n", rw->active_writers);
    fprintf(stderr, "waiting readers: %d\n", rw->waiting_readers);
    fprintf(stderr, "active readers: %d\n", rw->active_readers);

    fprintf(stderr, "readers in between: %d\n", rw->readers_in_between);

    // thinking through the logic: notes
    // if (rw->priority == WRITERS && (rw->active_readers > 0 || rw->active_writers > 0) ) wait
    // if (rw->priority == READERS && (rw->active_readers > 0 || rw->waiting_readers > 0) ) wait
    //                             // active readers = wait     // OR if there are waiting readers and not enough have gone
    // if (rw->priority == N_WAY && (rw->active_readers > 0 || ( rw->waiting_readers > 0 && rw->readers_in_between < rw->n ) ) )

    // combining the above conditions
    while (
        rw->active_readers > 0 || rw->active_writers > 0
        || (rw->priority == READERS && rw->waiting_readers > 0)
        || (rw->priority == N_WAY && (rw->waiting_readers > 0 && rw->readers_in_between < rw->n))) {

        pthread_cond_wait(&rw->writers_go, &rw->mutex);
    }

    rw->waiting_writers--; // it stopped waiting,
    rw->active_writers++; // and now it's writing

    rw->readers_in_between = 0; // reset readers in between

    pthread_mutex_unlock(&rw->mutex); // release lock
}

// a thread is finished writing! itll let go of it's lock and signal other threads... using this function:
void writer_unlock(rwlock_t *rw) {

    // gonna mess w struct fields --> get a lock
    pthread_mutex_lock(&rw->mutex);

    rw->active_writers--; // this thread has finished reading

    // signal the next threads
    signal_next(rw);

    pthread_mutex_unlock(&rw->mutex); // release lock
}
