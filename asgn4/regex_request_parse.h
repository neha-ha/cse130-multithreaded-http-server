
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

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

    // key and value for content length header:

    // key ends with first instance of :
    // betweeen [1, 128] characters
    // only contains characters from [a-zA-Z0-9.-]
    char key[129];

    // max 128 characters, only from the set of printable ASCII characters
    char value[129];

    // key and value for request-id header:
    char rid_key[129];
    int rid_value; // initialized to 0 if not provided

    // message body (optional) ------------------------------------------------------------------------------------------

    // necessary for PUT requests-- and the HeaderField.key must be 'Content-Length', with the value being the # of bytes in the message body
    // illegal for GET requests
    // char message_body[2000];
    // NOTE: this worked, but was inefficient-- wasted 2000 of memory
    // NOTE: instead, just return the beginning index of the message body that u match lmfao
};

int populate(struct Request *r, char *buffer);
