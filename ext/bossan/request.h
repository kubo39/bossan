#ifndef REQUEST_H
#define REQUEST_H

#include <inttypes.h>
#include "buffer.h"

#define LIMIT_PATH 1024 * 4
#define LIMIT_FRAGMENT 1024
#define LIMIT_URI 1024 * 4
#define LIMIT_QUERY_STRING 1024 * 8

#define LIMIT_REQUEST_FIELDS 30 
#define LIMIT_REQUEST_FIELD_SIZE 1024 * 8 

typedef enum {
  BODY_TYPE_NONE,
  BODY_TYPE_TMPFILE,
  BODY_TYPE_BUFFER
} request_body_type;

typedef enum {
  FIELD,
  VAL,
} field_type;

typedef struct {
  buffer *field;
  buffer *value;
} header;

typedef struct {
  buffer *path;
  buffer *uri;
  buffer *query_string;
  buffer *fragment;
  header *headers[LIMIT_REQUEST_FIELDS];
  uint32_t num_headers;
  field_type last_header_element;   
} request;


request *
new_request(void);

header *
new_header(size_t fsize, size_t flimit, size_t vsize, size_t vlimit);

void
free_header(header *h);

void
free_request(request *req);

#endif
