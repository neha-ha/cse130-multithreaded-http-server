ASSIGNMENT 3

queue.c
- implemented in much the same way as the lecture slides-- citation to prof veenstra!
- used two semaphores, sem_t full_cells and sem_t empty_cells to handle blocking when buffer is full or empty
- instead of using a binary semaphore as a lock to protect buffer data, i just used an actual lock

Makefile
- provided in resources

rwlock.c
- struct has fields for active_readers, active_writers, waiting_readers, waiting_writers, readers_in_between-- all initialized to 0
- 2 condition variables, one for when writers can go and one for when readers can go
- mutex to protect the struct fields above
- this is all inspired heavily by the implementation in Tom Anderson and Mike Dahlin’s Operating Systems: Principle
and Practice, Volume 2
- readers_in_between was my own creation so that I could ensure N-way priority
- all unlocking logic (which thread type to signal next) is outsourced to a single function, signal_next that is called in both writer_unlock and reader_unlock 
- i chose to use convars and locks instead of semaphores simply because the logic made more sense to me-- plus, i used semaphores in queue.c so why not get some practice with the other methods?

rwlock.h and queue.h
- provided in resources

