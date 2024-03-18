#include "regex_request_parse.h"

// uses regex to parse the fields and fills Request appropriately
// returns -1 for 400 bad request error code in the case of incorrect formatting
// if all OK, returns the beginning index of the message body
int populate(struct Request *r, char *buffer) {

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
        //fprintf(stderr, "error0parse\n");
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
            //fprintf(stderr, "error1parse\n");
            return -1;
        }

        // check if it's relevant to us-- we only care about content-length & request-id
        char keytemp[129] = { "\0" };
        strncpy(keytemp, currentpos + header_matches[1].rm_so,
            header_matches[1].rm_eo - header_matches[1].rm_so);

        if (strcmp(keytemp, "Content-Length") == 0) {
            //let's memorialize in the struct
            strncpy(r->key, currentpos + header_matches[1].rm_so,
                header_matches[1].rm_eo - header_matches[1].rm_so);
            strncpy(r->value, currentpos + header_matches[2].rm_so,
                header_matches[2].rm_eo - header_matches[2].rm_so);
        } else if (strcmp(keytemp, "Request-Id") == 0) {
            //let's memorialize in the struct
            strncpy(r->rid_key, currentpos + header_matches[1].rm_so,
                header_matches[1].rm_eo - header_matches[1].rm_so);
            char ridvalue[129] = { "\0" };
            strncpy(ridvalue, currentpos + header_matches[2].rm_so,
                header_matches[2].rm_eo - header_matches[2].rm_so);
            r->rid_value = atoi(ridvalue);
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
            //fprintf(stderr, "error2parse\n");
            return -1;
        }

        //strncpy(r->message_body, currentpos + message_matches[1].rm_so,
        //message_matches[1].rm_eo - message_matches[1].rm_so);

    } else {
        // if no \r\n\r\n at end, throw error!
        regfree(&request_regex);
        regfree(&header_regex);
        regfree(&message_regex);
        //fprintf(stderr, "error3parse\n");
        return -1;
    }

    // if we reach this point, everything worked gg
    regfree(&request_regex);
    regfree(&header_regex);
    regfree(&message_regex);
    //fprintf(stderr, "noerror\n");
    //  so return the start index of the message
    return finalpos + 2; // + 2 to account for the \r\n after the last header match
}
