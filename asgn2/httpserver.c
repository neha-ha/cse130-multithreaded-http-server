#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <regex.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "asgn2_helper_funcs.h"

// get buffer set up
#define BUFFER_SIZE 8000
char buffer[BUFFER_SIZE];

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

// ADDING AN EXTRA 1 TO THE MAX SIZE OF EVERY FIELD SO THAT THERE'S STILL ROOM FOR THE NULL TERMINATOR AT THE END OF EACH--- OTHERWISE IT FUCKING TWEAKS

// set up a struct for a request
struct Request {

    // request line (mandatory) -------------------------------------------------------------------------------------

    // contains at most 8 characters from range [a-zA-z]
    // this is the GET and PUT section.. so really only 3 char max
    char method[9];

    // starts with '/'.
    // has between [2, 64] characters total.
    // all other characters besides '/' are in [a-zA-Z0-9.-]
    // represents the file u want to read / write to
    // directories NOT allowed
    char URI[65];

    // has the format HTTP/#.#
    // each # is a SINGLE DIGIT number
    // our server only implements version 1.1, so this really can only be HTTP/1.1
    char version[9];

    // header field (optional, 0 or more) -----------------------------------------------------------------------------

    // key ends with first instance of :
    // betweeen [1, 128] characters
    // only contains characters from [a-zA-Z0-9.-]
    char key[129];

    // max 128 characters, only from the set of printable ASCII characters
    char value[129];

    // message body (optional) ------------------------------------------------------------------------------------------

    // necessary for PUT requests-- and the HeaderField.key must be 'Content-Length', with the value being the # of bytes in the message body
    // illegal for GET requests
    // char message_body[2000];
    // NOTE: this worked, but was inefficient-- wasted 2000 of memory
    // NOTE: instead, just return the beginning index of the message body that u match lmfao
};
// uses regex to parse the fields and fills Request appropriately
// returns -1 for 400 bad request error code in the case of incorrect formatting
// if all OK, returns the beginning index of the message body
int populate(struct Request *r) {

    // capture groups are in parenthesis to get the designated fields
    // method         //uri                 //version
    const char *request_line_pattern
        = "^([a-zA-Z]{1,8}) /([a-zA-Z0-9.-]{1,63}) (HTTP/[0-9][.][0-9])\r\n";
    // space is 32 in ascii, ~ is 126. so [ -~] includes all char between 32 and 126 in ASCII, aka all printable characters
    const char *header_field_pattern = "([a-zA-Z0-9.-]{1,128}): ([ -~]{1,128})\r\n";
    const char *message_body_pattern = "\r\n(.*)";

    // create regex_t objects for the patterns
    regex_t request_regex, header_regex, message_regex;

    // compile the regex patterns
    regcomp(&request_regex, request_line_pattern, REG_EXTENDED);
    regcomp(&header_regex, header_field_pattern, REG_EXTENDED);
    regcomp(&message_regex, message_body_pattern, REG_EXTENDED);

    // create arrays to store the match results
    regmatch_t request_matches[4];
    regmatch_t header_matches[3];
    regmatch_t message_matches[2];

    // match the request line
    int res = regexec(&request_regex, buffer, 4, request_matches, 0);

    // if nothing matched, return 400
    if (res != 0) {
        regfree(&request_regex);
        regfree(&header_regex);
        regfree(&message_regex);
        fprintf(stderr, "error0parse\n");
        return -1;
    }

    // otherwise, copy the matched strings over into the appropriate field
    // we initialized everything to be full of nulls, so we don't need to worry about null termination
    // starting position        // length of matched substring
    strncpy(r->method, buffer + request_matches[1].rm_so,
        request_matches[1].rm_eo - request_matches[1].rm_so);
    strncpy(r->URI, buffer + request_matches[2].rm_so,
        request_matches[2].rm_eo - request_matches[2].rm_so);
    strncpy(r->version, buffer + request_matches[3].rm_so,
        request_matches[3].rm_eo - request_matches[3].rm_so);

    // now match headers
    // start at the position in the buffer past the request line
    char *currentpos = buffer + request_matches[0].rm_eo;
    int finalpos = request_matches[0].rm_eo;

    //for each match
    while (regexec(&header_regex, currentpos, 3, header_matches, 0) == 0) {

        // make sure there are no bakwas header fields hiding before a real one
        //printf("currentpos\n%s\n", currentpos);
        //printf("currentpos +\n%s\n", currentpos + header_matches[0].rm_so);
        if (strcmp(currentpos, currentpos + header_matches[0].rm_so) != 0) {
            regfree(&request_regex);
            regfree(&header_regex);
            regfree(&message_regex);
            fprintf(stderr, "error1parse\n");
            return -1;
        }

        // check if it's relevant to us-- we only care about content-length
        char keytemp[129] = { "\0" };
        strncpy(keytemp, currentpos + header_matches[1].rm_so,
            header_matches[1].rm_eo - header_matches[1].rm_so);

        if (strcmp(keytemp, "Content-Length") == 0) {
            //let's memorialize in the struct
            strncpy(r->key, currentpos + header_matches[1].rm_so,
                header_matches[1].rm_eo - header_matches[1].rm_so);
            strncpy(r->value, currentpos + header_matches[2].rm_so,
                header_matches[2].rm_eo - header_matches[2].rm_so);
        }

        // move to next part of the string
        currentpos += header_matches[0].rm_eo;
        finalpos += header_matches[0].rm_eo;
    }

    // alr now time to match the message body
    // this initializes a return value for the number of bytes read into the message body at this stage
    //int returnvalue = 0; // if no message body, just return this

    if (regexec(&message_regex, currentpos, 2, message_matches, 0) == 0) {

        // make sure there are no bakwas header fields in there after a real one
        if (strcmp(currentpos, currentpos + message_matches[0].rm_so) != 0) {
            regfree(&request_regex);
            regfree(&header_regex);
            regfree(&message_regex);
            fprintf(stderr, "error2parse\n");
            return -1;
        }

        //strncpy(r->message_body, currentpos + message_matches[1].rm_so,
        //message_matches[1].rm_eo - message_matches[1].rm_so);

    } else {
        // if no \r\n\r\n at end, throw error!
        regfree(&request_regex);
        regfree(&header_regex);
        regfree(&message_regex);
        fprintf(stderr, "error3parse\n");
        return -1;
    }

    // if we reach this point, everything worked gg
    regfree(&request_regex);
    regfree(&header_regex);
    regfree(&message_regex);
    fprintf(stderr, "noerror\n");
    //  so return the start index of the message
    return finalpos + 2; // + 2 to account for the \r\n after the last header match
}

// writes file open error response to socket
void fileopen_error_handle(int socket) {
    if (errno == ENOENT) { // no such file or directory
        write_n_bytes(socket, response_404, strlen(response_404));
    } else if (errno == EACCES || errno == EISDIR) { // permission denied or is directory
        write_n_bytes(socket, response_403, strlen(response_403));
    } else { // other error
        write_n_bytes(socket, response_500, strlen(response_500));
    }
}

// main function, aka "scaffolding"-- handles the main looping process of
//  1) listening for incoming connections
//  2) accepting connections
//  3) processing requests
//  4) closing connections
int main(int argc, char **argv) {

    // example run:
    // on one terminal ./httpserver <port>
    // on another terminal tab printf "command" | ./cse130_nc localhost <port>
    // the server response will show up in terminal 2, and terminal 1 will have outputs from this program

    // get port # from commmand line argument
    // if too many arguments || out of bounds port number supplied, exit
    if (argc != 2 || atoi(argv[1]) < 1 || atoi(argv[1]) > 65535) {
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }
    // otherwise, save the port
    int port = atoi(argv[1]);

    // create a Listener_Socket
    Listener_Socket listener;

    // initialize it to listen at the port
    int listening = listener_init(&listener, port);
    // if errors, exit
    if (listening != 0) {
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }

    // start looping the main process
    while (1) {

        // listen for and accept any connections at that socket
        // listener_accept blocks execution until a new incoming connection is made to this port
        // then returns a socket
        //      socket-- pretty much a file descriptor, it's used to read / send bytes from / to the client
        int socket = listener_accept(&listener);

        // int flags = fcntl(socket, F_GETFL, 0);
        // fcntl(socket, F_SETFL, flags | O_NONBLOCK);

        // read bytes from the user- start with 2048 bytes or until /r/n/r/n to see if the request is chilling
        int initial_byte_read = read_until(socket, buffer, 2048, "\r\n\r\n");
        fprintf(stderr, "bytes read-until %d\n", initial_byte_read);

        // create a Request & initialize every field in it to 0
        struct Request r;
        memset(&r, 0, sizeof(struct Request));

        // use regex to populate the Request and it's subfields
        int message_body_start = populate(&r);
        // check for bad request error & write response if so
        if (message_body_start == -1) {
            write_n_bytes(socket, response_400, strlen(response_400));
            close(socket);
            continue;
        }

        // other error handling that arises with file opening etc
        if (strcmp(r.method, "GET") == 0) {

            // check version
            if (strcmp(r.version, "HTTP/1.1") != 0) {
                write_n_bytes(socket, response_505, strlen(response_505));
                close(socket);
                continue;
            }

            // opening file with rdwr to catch directory error
            int fd = open(r.URI, O_RDWR);
            // if it doesn't open, throw error depending on ERRNO
            if (fd == -1) {
                fileopen_error_handle(socket);
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

            close(fd);

        } else if (strcmp(r.method, "PUT") == 0) {

            // check version
            if (strcmp(r.version, "HTTP/1.1") != 0) {
                write_n_bytes(socket, response_505, strlen(response_505));
                close(socket);
                continue;
            }

            // check that there is a content header
            if (strcmp(r.key, "Content-Length") != 0) {
                write_n_bytes(socket, response_400, strlen(response_400));
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
                    fileopen_error_handle(socket);
                    close(socket);
                    continue;
                }
                newfile = true;
            }

            // write the bytes that are part of the message body, but have already been read from the socket
            int bytes_to_write = initial_byte_read - message_body_start;
            fprintf(stderr, "bytes to write %d\n", bytes_to_write);
            fprintf(stderr, "start index of messagebody %d\n", message_body_start);
            int written = write_n_bytes(fd, buffer + message_body_start, bytes_to_write);
            fprintf(stderr, "bytes written just now %d \n", written);
            fprintf(stderr, "content value %d \n", atoi(r.value));

            // write the rest of the message
            // while (written < atoi(r.value)) {
            //     written += pass_n_bytes(socket, fd, atoi(r.value) - written);
            // }
            int written2 = pass_n_bytes(socket, fd, atoi(r.value) - written);

            fprintf(stderr, "bytes written rest of message %d \n", written2);

            if (newfile) {
                write_n_bytes(socket, response_201, strlen(response_201));
            } else {
                write_n_bytes(socket, response_200_PUT, strlen(response_200_PUT));
            }

            // close file
            close(fd);

        } else { // unimplemented function
            write_n_bytes(socket, response_501, strlen(response_501));
        }

        //finally close connection
        memset(buffer, 0, BUFFER_SIZE);
        close(socket);
    }
}
