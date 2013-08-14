#ifndef CLIENT_H
#define CLIENT_H

#include "request.h"

typedef struct _client {
  int fd;
  char *remote_addr;
  uint32_t remote_port;

  uint8_t keep_alive;
  char upgrade;
  request *current_req;
  request_queue *request_queue;

  char complete;

  http_parser *http_parser;    // http req parser
  uint16_t status_code;            // response status code
    
  VALUE http_status;             // response status line
  VALUE headers;                 // http response headers
  uint8_t header_done;            // header write status
  VALUE response;                // rack response object
  VALUE response_iter;           // rack response object
  uint8_t chunked_response;     // use Transfer-Encoding: chunked
  uint8_t content_length_set;     // content_length_set flag
  uint32_t content_length;         // content_length
  uint32_t write_bytes;            // send body length
  void *bucket;               //write_data
  uint8_t response_closed;    //response closed flag

  int response_body_type;  // response body type
} client_t;

#endif
