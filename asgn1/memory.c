#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

//set up buffers for reading / writing to files
#define BUFFER_SIZE 9000
char buf[BUFFER_SIZE];

void freestrings(char *a, char *b) {
    free(a);
    free(b);
}

int main() {

    //we don't need to offer the user a prompt to get their input from stdin
    //the read function does that by default when using the stdin file descriptor

    //user commands can be in the form get\nfoo.txt\n, set\nfoo.txt\n12\nHello World!, or invalid\nfoo.txt, etc.
    //this means that each individual part can be entered as separate lines, i.e.
    // get
    // foo.txt
    //in this case, we need to loop the read command so that it can handle this

    //we must also handle partial reads caused by throttling, which keeps interrupting before you read BUFFER_SIZE amount
    //data from the command was literally being overwritten in the buffer
    //so lets create a char* to store what we r reading independent of buffer
    char *command = malloc(BUFFER_SIZE); //pre-allocating

    int bytes_read = 0;
    int bytes_read_total = 0;

    // but firstly, let's figure out what kind of command this is-- get? set? something not allowed?
    //we put this in a loop because of the throttling issue-- sometimes, it was only reading literally a character at a time. so. here we are
    do {

        bytes_read = read(STDIN_FILENO, buf, 1);

        if (bytes_read == -1) {
            break;
        }

        //add to the command
        command = realloc(command,
            bytes_read_total + bytes_read + 1); //realloc to fit the new data read in this iteration
        memcpy(command + bytes_read_total, buf, bytes_read);

        bytes_read_total += bytes_read;

    } while (bytes_read > 0 && bytes_read_total < 4);

    //based on what command it is, our paths diverge
    //why are we diverging rn instead of after we read the entire command?
    //because: i.e. set\nfoo.txt\n158430689548690458694587549\nhhreuhgiuerhguie......
    //with that massive ass command, we time out and use hellla hellla memory
    //plus the buffer doesn't even have that much room
    //so we have to write as we read for that
    //so...we split rn

    if (strncmp(command, "get", 3) == 0) {

        //continue reading the rest of the command-- get will never be that fat
        do {

            bytes_read = read(STDIN_FILENO, buf, BUFFER_SIZE);

            if (bytes_read == -1) {
                break;
            }

            //add to the command
            command
                = realloc(command, bytes_read_total + bytes_read
                                       + 1); //realloc to fit the new data read in this iteration
            memcpy(command + bytes_read_total, buf, bytes_read);

            bytes_read_total += bytes_read;

        } while (bytes_read > 0);

        //null terminate the command-- this ensures that the system doesn't go outside the bounds of allocated data
        //fixed the conditional jump issues
        command = realloc(command, bytes_read_total + 1);
        command[bytes_read_total] = '\0';

        //now command holds the entire command (theoretically buf as well if it wasn't throttled-- but we can't rely on that)
        //to determine which command it is, we can use C's strtok function
        //strtok splits a string by some delimiter-- in this case, as shown in the examples above, the delimiters are \n
        //it essentially replaces the \n with \0... which can cause some errors later on

        //to prevent this, let's first copy our buffer
        char *commandcopy = strdup(command);
        //get first part of command i.e. "get" or "set"
        strtok(command, "\n"); //flush it
        char *location;

        //get the next token from strtok-- the next segment (location)
        location = strtok(NULL, "\n");

        //check that there is a newline at the end
        if (commandcopy[bytes_read_total - 1] != '\n') {
            fprintf(stderr, "Invalid Command\n");
            freestrings(commandcopy, command);
            return 1;
        }

        //make sure there isn't an extra token provided-- like get\nfoo.txt\n12
        if (strtok(NULL, "\n") != NULL) {
            fprintf(stderr, "Invalid Command\n");
            freestrings(commandcopy, command);
            return 1;
        }

        //try to open the file location
        //opening as rdwr instead of read to catch directory error
        int fd = open(location, O_RDWR);

        //if it got fucked exit
        if (fd == -1) {
            fprintf(stderr, "Invalid Command\n");
            freestrings(commandcopy, command);
            close(fd);
            return 1;
        }

        //otherwise, read the contents

        //we are working with binary files as well, which can contain \0
        //in that case, one read will not read everything it needs to
        //we need to loop read in order to get it all
        //moreover, with big ass files like 1gb+, we can't store all of it in the buffer
        //so we need to write as we read

        while ((bytes_read = read(fd, buf, BUFFER_SIZE)) > 0) {

            int bytes_written_total = 0;
            //write what's been read for now
            do {
                int bytes_written = write(
                    STDOUT_FILENO, buf + bytes_written_total, bytes_read - bytes_written_total);

                if (bytes_written < 0) {
                    fprintf(stderr, "Operation Failed\n");
                    freestrings(commandcopy, command);
                    close(fd);
                    return 1;
                }

                bytes_written_total += bytes_written;
            } while (bytes_written_total < bytes_read);
        }

        // close file
        close(fd);
        freestrings(commandcopy, command);
        return 0;

    } else if (strncmp(command, "set", 3) == 0) {

        //just like in get, we need to finish reading the rest of our command
        //we gonna read 1 by 1, monitoring the \n we  hit-- thats how we know key info like bytes, location, etc.
        //i.e. set\nfoo.txt\n12\nHello World!
        int newlines = 0;
        do {

            bytes_read = read(STDIN_FILENO, buf, 1);

            if (bytes_read == -1) {
                break;
            }

            //add to the command
            command
                = realloc(command, bytes_read_total + bytes_read
                                       + 1); //realloc to fit the new data read in this iteration
            memcpy(command + bytes_read_total, buf, bytes_read);

            bytes_read_total += bytes_read;

            //check newlines
            if (command[bytes_read_total - 1] == '\n') {
                newlines++;
            }

            //when we have 2, we have our location and bytes so we can kill this loop
            if (newlines == 2) {
                break;
            }

        } while (bytes_read > 0);

        //null terminate the command-- this ensures that the system doesn't go outside the bounds of allocated data
        //fixed the conditional jump issues
        command = realloc(command, bytes_read_total + 1);
        command[bytes_read_total] = '\0';

        //now command holds the entire command BESIDES WHAT WE NEED TO WRITE
        //to determine which command it is, we can use C's strtok function
        //strtok splits a string by some delimiter-- in this case, as shown in the examples above, the delimiters are \n
        //it essentially replaces the \n with \0... which can cause some errors later on

        //to prevent this, let's first copy our buffer
        char *commandcopy = strdup(command);
        //get first part of command i.e. "get" or "set".. flush it
        strtok(command, "\n");
        char *location;
        char *bytes;

        //get the rest of the tokens from the command
        location = strtok(NULL, "\n");
        bytes = strtok(NULL, "\n");

        //if any of these required tokens are missing, exit
        if (location == NULL || bytes == NULL) {
            fprintf(stderr, "Invalid Command\n");
            freestrings(commandcopy, command);
            return 1;
        }

        //open file to write-- if it does not exist, create it. if it exists, erase it (truncate flag)
        int fd = open(location, O_CREAT | O_RDWR | O_TRUNC, 0666);

        //if it got fucked, exit
        if (fd == -1) {
            fprintf(stderr, "Invalid Command\n");
            freestrings(commandcopy, command);
            return 1;
        }

        //however, between bytes and content length, we must choose the shorter to write
        int bytes_to_write = atoi(bytes);
        fprintf(stderr, "%d \n", bytes_to_write);

        bytes_read_total = 0;
        while ((bytes_read = read(STDIN_FILENO, buf, BUFFER_SIZE)) >= 0
               && bytes_read_total < bytes_to_write) {

            //set up appropriate number of bytes to write this iteration
            int bytes_to_write_rn = bytes_read;
            if (bytes_read_total + bytes_read > bytes_to_write) {
                bytes_to_write_rn = bytes_to_write - bytes_read_total;
            }

            //write them
            int bytes_written_total = 0;
            while (bytes_written_total < bytes_to_write_rn) {

                int bytes_written = write(fd, buf + bytes_written_total, bytes_to_write_rn);

                if (bytes_written < 0) {
                    fprintf(stderr, "Operation Failed\n");
                    freestrings(commandcopy, command);
                    close(fd);
                    return 1;
                }

                bytes_written_total += bytes_written;
            }

            bytes_read_total += bytes_read;
        }

        //write a sucess message to stdout
        fprintf(stdout, "OK\n");
        fprintf(stderr, "%d \n", bytes_read_total);

        // close file
        close(fd);
        freestrings(commandcopy, command);
        return 0;

    }
    //invalid command, exit
    else {
        fprintf(stderr, "Invalid Command\n");
        free(command);
        return 1;
    }
}
