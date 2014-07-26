#pragma once

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
  buffer_t *path;
  uint32_t num_headers;
  field_type last_header_element;

  VALUE environ;
  void *next;
  int body_length;
  int body_readed;
  int bad_request_code;
  void *body;
  request_body_type body_type;
    
  VALUE field;
  VALUE value;
  uintptr_t start_msec;
} request;

typedef struct {
  int size;
  request *head;
  request *tail;
} request_queue;


request *
new_request(void);

void push_request(request_queue *q, request *req);

request* shift_request(request_queue *q);

request_queue* new_request_queue(void);

void free_request_queue(request_queue *q);

void
free_request(request *req);
