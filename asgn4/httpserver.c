#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <pthread.h>

#include "asgn2_helper_funcs.h"

// asgn4 helper headers- these all outline functions in asgn4_helper_funcs.a
#include "debug.h"
#include "protocol.h"
#include "queue.h"
#include "rwlock.h"

// self-created helper files
#include "linkedlist.h"
#include "regex_request_parse.h"

// define constant parts of return strings
#define response_200     "HTTP/1.1 200 OK\r\nContent-Length: " //21\r\n\r\nHello World, I am foo (GET)
#define response_200_PUT "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n"
#define response_201     "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n"
#define response_400     "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n"
#define response_403     "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n"
#define response_404     "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n"
#define response_500                                                                               \
    "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal Server Error\n"
#define response_501 "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n"
#define response_505                                                                               \
    "HTTP/1.1 505 Version Not Supported\r\nContent-Length: 22\r\n\r\nVersion Not Supported\n"

// writes file open error response to socket
void fileopen_error_handle(int socket, struct Request *r) {
    if (errno == ENOENT) { // no such file or directory
        write_n_bytes(socket, response_404, strlen(response_404));
        // audit log
        fprintf(stderr, "%s,/%s,%d,%d\n", r->method, r->URI, 404, r->rid_value);
    } else if (errno == EACCES || errno == EISDIR) { // permission denied or is directory
        write_n_bytes(socket, response_403, strlen(response_403));
        // audit log
        fprintf(stderr, "%s,/%s,%d,%d\n", r->method, r->URI, 403, r->rid_value);
    } else { // other error
        write_n_bytes(socket, response_500, strlen(response_500));
        // audit log
        fprintf(stderr, "%s,/%s,%d,%d\n", r->method, r->URI, 500, r->rid_value);
    }
}

#define BUFFER_SIZE 3000
List linkedlist;
queue_t *q;
pthread_mutex_t mutex;

//----------------------------------------------------------------------------------------------------------------------------------------

// function that each thread executes to perform the tast
void *work() {

    // create buffer
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    while (1) {

        // pop a socket from the queue
        int socket;
        // waits until a socket can be popped as part of the multithreaded queue functionality
        queue_pop(q, (void **) &socket);

        //fprintf(stdout, "popped from queue!\n");
        fflush(stdout);

        // read bytes from the user- start with 2048 bytes or until /r/n/r/n to see if the request is chilling
        int initial_byte_read = read_until(socket, buffer, 2048, "\r\n\r\n");
        //fprintf(stderr, "bytes read-until %d\n", initial_byte_read);
        //fprintf(stdout, "this is what was read:\n %s\n", buffer);
        fflush(stdout);

        // create a Request & initialize every field in it to 0
        struct Request r;
        memset(&r, 0, sizeof(struct Request));
        //fprintf(stderr, "struct created\n");

        // use regex to populate the Request and it's subfields
        int message_body_start = populate(&r, buffer);
        //fprintf(stderr, "message body start index! %d \n", message_body_start);
        //fprintf(stderr, "uri %s\n", r.URI);
        // check for bad request error & write response if so
        if (message_body_start == -1) {
            write_n_bytes(socket, response_400, strlen(response_400));
            // audit log
            fprintf(stderr, "%s,/%s,%d,%d\n", r.method, r.URI, 400, r.rid_value);
            close(socket);
            continue;
        }

        // find the file's associated rwlock, create new if none exists
        rwlock_t *rwlock = NULL;

        //fprintf(stderr, "linked list size %d\n", length(linkedlist));

        pthread_mutex_lock(&mutex);

        // go through linked list
        moveFront(linkedlist);
        while (indexx(linkedlist) != -1) {
            char *filename = (char *) getfilename(linkedlist);
            fprintf(stdout, "filename of lock %s, uri %s \n", filename, r.URI);
            fflush(stdout);
            if (strcmp((char *) getfilename(linkedlist), r.URI) == 0) {
                fprintf(stdout, "got lock from linkedlist\n");
                fflush(stdout);
                rwlock = (rwlock_t *) getlock(linkedlist);
                break;
            }
            moveNext(linkedlist);
        }

        fprintf(stdout, "length after loop  %d \n", length(linkedlist));
        fflush(stdout);

        // if file was not in linked list, add it & a new rwlock
        if (rwlock == NULL) {
            fprintf(stdout, "new lock created \n");
            fflush(stdout);
            // new rwlock with N-WAY (2) priority with n being 1
            rwlock = rwlock_new(2, 1);
            append(linkedlist, r.URI, rwlock);
        }

        fprintf(stdout, "length after potential lock creation  %d \n", length(linkedlist));
        fflush(stdout);

        //fprintf(stderr, "linked list size %d\n", length(linkedlist));

        // unlock from linked list
        pthread_mutex_unlock(&mutex);

        //fprintf(stdout, "unlocked from mutex \n");
        //fflush(stdout);

        // begin to actually complete the request

        // other error handling that arises with file opening etc
        if (strcmp(r.method, "GET") == 0) {

            // acquire reader lock
            reader_lock(rwlock);

            //fprintf(stdout, "reader lock acquired \n");
            //fflush(stdout);

            // check version
            if (strcmp(r.version, "HTTP/1.1") != 0) {
                write_n_bytes(socket, response_505, strlen(response_505));
                // audit log
                fprintf(stderr, "%s,/%s,%d,%d\n", r.method, r.URI, 505, r.rid_value);
                reader_unlock(rwlock);
                close(socket);
                continue;
            }

            // opening file with rdwr to catch directory error
            int fd = open(r.URI, O_RDWR);
            // if it doesn't open, throw error depending on ERRNO
            if (fd == -1) {
                fileopen_error_handle(socket, &r);
                reader_unlock(rwlock);
                close(socket);
                continue;
            }

            // get file size in bytes
            struct stat fileStat;
            fstat(fd, &fileStat);
            uint64_t filesize = fileStat.st_size;
            // printf("%ld \n", filesize);

            // convert filesize to string
            char fsize[129] = { "\0" };
            sprintf(fsize, "%lu", filesize);

            // write first part of response
            write_n_bytes(socket, response_200, strlen(response_200));
            write_n_bytes(socket, fsize, strlen(fsize));
            write_n_bytes(socket, "\r\n\r\n", 4);

            pass_n_bytes(fd, socket, filesize);

            // audit log
            fprintf(stderr, "%s,/%s,%d,%d\n", r.method, r.URI, 200, r.rid_value);

            // finished reading, unlock
            reader_unlock(rwlock);
            //fprintf(stdout, "reader unlocked \n");
            //fflush(stdout);

            close(fd);

        } else if (strcmp(r.method, "PUT") == 0) {

            // acquire writer lock
            writer_lock(rwlock);
            //fprintf(stdout, "writer locked \n");
            //fflush(stdout);

            // check version
            if (strcmp(r.version, "HTTP/1.1") != 0) {
                write_n_bytes(socket, response_505, strlen(response_505));
                // audit log
                fprintf(stderr, "%s,/%s,%d,%d\n", r.method, r.URI, 505, r.rid_value);
                writer_unlock(rwlock);
                close(socket);
                continue;
            }

            // check that there is a content header
            if (strcmp(r.key, "Content-Length") != 0) {
                write_n_bytes(socket, response_400, strlen(response_400));
                // audit log
                fprintf(stderr, "%s,/%s,%d,%d\n", r.method, r.URI, 400, r.rid_value);
                writer_unlock(rwlock);
                close(socket);
                continue;
            }

            bool newfile = false;

            // open file to write-- this will work only if it already exists
            int fd = open(r.URI, O_WRONLY | O_TRUNC, 0666);
            if (fd == -1) {
                // if it fails, open with the create flag
                // open file to write-- if it does not exist, create it. if it exists, erase it (truncate flag)
                fd = open(r.URI, O_CREAT | O_WRONLY | O_TRUNC, 0666);
                // if it doesn't open, throw error depending on ERRNO
                if (fd == -1) {
                    fileopen_error_handle(socket, &r);
                    writer_unlock(rwlock);
                    close(socket);
                    continue;
                }
                newfile = true;
            }

            // write the bytes that are part of the message body, but have already been read from the socket
            int bytes_to_write = initial_byte_read - message_body_start;
            //fprintf(stderr, "bytes to write %d\n", bytes_to_write);
            //fprintf(stderr, "start index of messagebody %d\n", message_body_start);
            int written = write_n_bytes(fd, buffer + message_body_start, bytes_to_write);
            //fprintf(stderr, "bytes written just now %d \n", written);
            //fprintf(stderr, "content value %d \n", atoi(r.value));

            // write the rest of the message
            pass_n_bytes(socket, fd, atoi(r.value) - written);

            //fprintf(stderr, "bytes written rest of message %d \n", written2);

            if (newfile) {
                write_n_bytes(socket, response_201, strlen(response_201));
                // audit log
                fprintf(stderr, "%s,/%s,%d,%d\n", r.method, r.URI, 201, r.rid_value);

            } else {
                write_n_bytes(socket, response_200_PUT, strlen(response_200_PUT));
                // audit log
                fprintf(stderr, "%s,/%s,%d,%d\n", r.method, r.URI, 200, r.rid_value);
            }

            // finished writing, unlock
            writer_unlock(rwlock);
            //fprintf(stdout, "writer unlocked \n");
            //fflush(stdout);

            // close file
            close(fd);

        } else { // unimplemented function
            write_n_bytes(socket, response_501, strlen(response_501));
            // audit log
            fprintf(stderr, "%s,/%s,%d,%d\n", r.method, r.URI, 501, r.rid_value);
        }

        //finally close connection
        // read any remaining bytes in the buffer
        memset(buffer, 0, BUFFER_SIZE);
        memset(&r, 0, sizeof(struct Request));
        close(socket);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------

// let's begin with the dispatcher thread-- our main
int main(int argc, char **argv) {

    // first, we get the required information from the command
    // i.e. ./httpserver -t 5 1234
    // 5 is the number of threads, and 1234 is the port number to listen at

    // get thread number
    int opt = 0;
    int threads = 4; // if no thread # specified, use default of 4
    // the : after t signal that there is an argument after -t
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        // if the t option is found
        if (opt == 't') {
            threads = atoi(optarg); // set threads to the t option's argument
        } else {
            fprintf(stderr, "incorrect command");
            return -1;
        }
    }

    // -------------- some explanation for port # logic --------------------------------------------------------------------------------------------------------

    // after calling getopt, optind points to the index of the next argument that is not an option
    // so in this case, optind should point to the index of 1234

    //     0         1 2   3
    // ./httpserver -t 5 1234
    // 1234 is at index 3 if threads provided, optind is 3
    // and argc would be 4

    //     0         1
    // ./httpserver 1234
    // 1234 is at index 1 if no threads provided, optind is 1
    // and argc would be 2

    //     0         1 2
    // ./httpserver -t 5
    // optind would be 3, but no port provided
    // and argc would be 3

    // so, if optind >= argc, no port is provided, and we should quit
    // otherwise, the port # is always yielded by argv[optind]

    // ------------------------------------------------------------------------------------------------------------------------------------------------------

    // get port #
    // if none supplied, quit
    if (optind >= argc) {
        fprintf(stderr, "no port number");
        return -1;
    }
    //otherwise,
    int port = atoi(argv[optind]);

    // now that fields are parsed, we can set up

    // 1) create a Listener_Socket
    Listener_Socket listener;

    // initialize it to listen at the port
    int listening = listener_init(&listener, port);
    // if errors, exit
    if (listening != 0) {
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }

    // 2) create a threadsafe queue to manage incoming requests, max size being the number of threads
    q = queue_new(threads);

    // 3) create a linked list to manage the rwlocks for each file
    linkedlist = newList();

    // and a lock to handle the modification of linked list
    pthread_mutex_t mutex;
    // initialize  this mutex  with default attributes
    pthread_mutex_init(&mutex, NULL);

    // and corresponding condition variable
    //pthread_cond_t condition;
    // initialize      this cv  with default attributes
    //pthread_cond_init(condition, NULL);

    // 4) create worker threads
    pthread_t workers[threads]; // array of pthreads
    for (int i = 0; i < threads; ++i) {
        // create worker, default attributes, to execute the function work
        pthread_create(&workers[i], NULL, work, NULL);
    }

    // begin listening at the port for connections
    while (1) {

        // listen for and accept any connections at that socket
        // listener_accept blocks execution until a new incoming connection is made to this port
        // then returns a socket
        //      socket-- pretty much a file descriptor, it's used to read / send bytes from / to the client
        uintptr_t socket = listener_accept(&listener);
        //fprintf(stdout, "got connection, this is socket %d\n", socket);
        fflush(stdout);

        // add (push) this socket to the queue so that one of the worker threads can pick it up (pop)
        // waits until it can push if queue is full (multithreaded queue built in functionality)
        queue_push(q, (void *) socket);
        //fprintf(stdout, "pushed to queue \n");
        fflush(stdout);
    }
}
