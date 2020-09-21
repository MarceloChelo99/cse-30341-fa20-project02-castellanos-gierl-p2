/* request.c: Request structure */

#include "mq/request.h"

#include <stdlib.h>
#include <string.h>

/**
 * Create Request structure.
 * @param   method      Request method string.
 * @param   uri         Request uri string.
 * @param   body        Request body string.
 * @return  Newly allocated Request structure.
 */
Request * request_create(const char *method, const char *uri, const char *body) {
    Request *r = calloc(1, sizeof(Request));
    if (r == NULL) exit(1);

    r->method   = (char *) method;
    r->uri      = (char *) uri;
    r->body     = (char*) body;

    return r;
}

/**
 * Delete Request structure.
 * @param   r           Request structure.
 */
void request_delete(Request *r) {
    free(r->next);
    free(r);
}

/**
 * Write HTTP Request to stream:
 *  
 *  $METHOD $URI HTTP/1.0\r\n
 *  Content-Length: Length($BODY)\r\n
 *  \r\n
 *  $BODY
 *      
 * @param   r           Request structure.
 * @param   fs          Socket file stream.
 */
void request_write(Request *r, FILE *fs) {
   fprintf(fs, "%s %s HTTP/1.0\r\n\
                Content-Length: %lu\r\n\
                \r\n\
                %s",
                r->method, r->uri, strlen(r->body), r->body); 
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */ 
