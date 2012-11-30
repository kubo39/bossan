#include "ruby.h"
#include <assert.h>
#include <fcntl.h>   
#include <stddef.h> 
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include "time_cache.h"
#include "http_parser.h"
#include "picoev.h"

#define MAX_FDS 1024 * 8
#define ACCEPT_TIMEOUT_SECS 1
#define SHORT_TIMEOUT_SECS 2
#define WRITE_TIMEOUT_SECS 5
#define READ_LONG_TIMEOUT_SECS 5

#define BACKLOG 1024
#define MAX_BUFSIZE 1024 * 8
#define INPUT_BUF_SIZE 1024 * 8

#define LIMIT_MAX 1024 * 1024 * 1024

#define LIMIT_SIZE 1024 * 512

#define LIMIT_PATH 1024 * 4
#define LIMIT_FRAGMENT 1024
#define LIMIT_URI 1024 * 4
#define LIMIT_QUERY_STRING 1024 * 8

#define LIMIT_REQUEST_FIELDS 30 
#define LIMIT_REQUEST_FIELD_SIZE 1024 * 8 

#define CRLF "\r\n"
#define DELIM ": "

#define MSG_500 ("HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\nServer:  " SERVER "\r\n\r\n<html><head><title>500 Internal Server Error</title></head><h1>Internal Server Error</h1><p>The server encountered an internal error and was unable to complete your request.  Either the server is overloaded or there is an error in the application.</p></html>\n")

#define MSG_400 ("HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\nServer: " SERVER "\r\n\r\n<html><head><title>Bad Request</title></head><body><p>Bad Request.</p></body></html>")

#define MSG_411 ("HTTP/1.0 411 Length Required\r\nContent-Type: text/html\r\nServer: " SERVER "\r\n\r\n<html><head><title>Length Required</title></head><body><p>Length Required.</p></body></html>")

#define MSG_413 ("HTTP/1.0 413 Request Entity Too Large\r\nContent-Type: text/html\r\nServer: " SERVER "\r\n\r\n<html><head><title>Request Entity Too Large</title></head><body><p>Request Entity Too Large.</p></body></html>")

#define SERVER "bossan/0.1"

VALUE server; // Bossan::Server

static VALUE version_key;
static VALUE version_val;
static VALUE scheme_key;
static VALUE scheme_val;
static VALUE errors_key;
static VALUE errors_val;
static VALUE multithread_key;
static VALUE multithread_val;
static VALUE multiprocess_key;
static VALUE multiprocess_val;
static VALUE run_once_key;
static VALUE run_once_val;

static VALUE script_key;
static VALUE script_val;
static VALUE server_name_key;
static VALUE server_name_val;
static VALUE server_port_key;
static VALUE server_port_val;

static VALUE server_protocol;
static VALUE path_info;
static VALUE request_uri;
static VALUE query_string;
static VALUE http_fragment;
static VALUE request_method;
static VALUE rb_remote_addr;
static VALUE rb_remote_port;
static VALUE rack_input;
static VALUE http_connection;

static VALUE empty_string;

static VALUE http_user_agent;

static VALUE i_keys;
static VALUE i_call;
static VALUE i_new;
static VALUE i_key;

static char *server_name = "127.0.0.1";
static short server_port = 8000;

static int listen_sock;  // listen socket

static int loop_done; // main loop flag
static picoev_loop* main_loop; //main loop
static VALUE rack_app = NULL; //rack app

static char *log_path = NULL; //access log path
static int log_fd = -1; //access log
static char *error_log_path = NULL; //error log path
static int err_log_fd = -1; //error log

static int is_keep_alive = 0; //keep alive support
int max_content_length = 1024 * 1024 * 16; //max_content_length

static VALUE StringIO;

typedef enum {
  WRITE_OK,
  MEMORY_ERROR,
  LIMIT_OVER,
} buffer_result;

typedef struct {
  char *buf;
  size_t buf_size;
  size_t len;
  size_t limit;
} buffer;

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

typedef enum {
  BODY_TYPE_NONE,
  BODY_TYPE_TMPFILE,
  BODY_TYPE_BUFFER
} request_body_type;

typedef struct _client {
  int fd;
  char *remote_addr;
  uint32_t remote_port;
  uint8_t keep_alive;
  request *req;
  uint32_t body_length;
  int body_readed;
  void *body;
  int bad_request_code;
  request_body_type body_type;    
  uint8_t complete;

  http_parser *http;          // http req parser
  VALUE environ;              // rack environ
  int status_code;            // response status code
    
  VALUE http_status;             // response status line
  VALUE headers;                 // http response headers
  uint8_t header_done;            // header write status
  VALUE response;                // rack response object
  VALUE response_iter;           // rack response object
  uint8_t content_length_set;     // content_length_set flag
  uint32_t content_length;         // content_length
  uint32_t write_bytes;            // send body length
  void *bucket;               //write_data
  uint8_t response_closed;    //response closed flag
} client_t;

typedef struct iovec iovec_t;

typedef struct {
  int fd;
  iovec_t *iov;
  uint32_t iov_cnt;
  uint32_t iov_size;
  uint32_t total;
  uint32_t total_size;
} write_bucket;

inline buffer *
new_buffer(size_t buf_size, size_t limit)
{
  buffer *buf;
  buf = ruby_xmalloc(sizeof(buffer));
  memset(buf, 0, sizeof(buffer));
  buf->buf = ruby_xmalloc(sizeof(char) * buf_size);
  buf->buf_size = buf_size;
  if(limit){
    buf->limit = limit;
  }else{
    buf->limit = LIMIT_MAX;
  }
  return buf;
}

inline buffer_result
write2buf(buffer *buf, const char *c, size_t  l) {
  size_t newl;
  char *newbuf;
  buffer_result ret = WRITE_OK;
  newl = buf->len + l;
    
  if (newl >= buf->buf_size) {
    buf->buf_size *= 2;
    if(buf->buf_size <= newl) {
      buf->buf_size = (int)(newl + 1);
    }
    if(buf->buf_size > buf->limit){
      buf->buf_size = buf->limit + 1;
    }
    newbuf = (char*)ruby_xrealloc(buf->buf, buf->buf_size);
    buf->buf = newbuf;
  }
  if(newl >= buf->buf_size){
    l = buf->buf_size - buf->len -1;
    ret = LIMIT_OVER;
  }
  memcpy(buf->buf + buf->len, c , l);
  buf->len += (int)l;
  return ret;
}

inline void
free_buffer(buffer *buf)
{
  ruby_xfree(buf->buf);
  ruby_xfree(buf);
}

inline VALUE
getRbString(buffer *buf)
{
  VALUE o;
  o = rb_str_new(buf->buf, buf->len);
  free_buffer(buf);
  return o;
}

inline char *
getString(buffer *buf)
{
  buf->buf[buf->len] = '\0';
  return buf->buf;
}

int
open_log_file(const char *path)
{
  return open(path, O_CREAT|O_APPEND|O_WRONLY, 0744);
}

static inline int
write_log(const char *new_path, int fd, const char *data, size_t len)
{
  int openfd;
  flock(fd, LOCK_EX);
    
  if(write(fd, data, len) < 0){
    flock(fd, LOCK_UN);
    //reopen
    openfd = open_log_file(new_path);
    if(openfd < 0){
      //fail
      return -1; 
    }
    flock(openfd, LOCK_EX);
    if(write(openfd, data, len) < 0){
      flock(openfd, LOCK_UN);
      // write fail
      return -1;
    }
    flock(openfd, LOCK_UN);
    return openfd;
  }
  flock(fd, LOCK_UN);
  return fd;
}

int 
write_access_log(client_t *cli, int log_fd, const char *log_path)
{
  char buf[1024];
  if(log_fd > 0){
    VALUE obj;
    char *method, *path, *version, *ua;
        
    obj = rb_hash_aref(cli->environ, request_method);
    if(obj){
      method = StringValuePtr(obj);
    }else{
      method = "-";
    }
                
    obj = rb_hash_aref(cli->environ, path_info);
    if(obj){
      path = StringValuePtr(obj);
    }else{
      path = "-";
    }
    
    obj = rb_hash_aref(cli->environ, server_protocol);
    if(obj){
      version = StringValuePtr(obj);
    }else{
      version = "-";
    }

    obj = rb_hash_aref(cli->environ, http_user_agent);
    if(obj){
      ua = StringValuePtr(obj);
    }else{
      ua = "-";
    }
    //update
    cache_time_update();
        
    sprintf(buf, "%s - - [%s] \"%s %s %s\" %d %d \"-\" \"%s\"\n", 
	    cli->remote_addr,
	    http_log_time,
	    method,
	    path,
	    version,
	    cli->status_code,
	    cli->write_bytes,
	    ua);
    return write_log(log_path, log_fd, buf, strlen(buf));
  }
  return 0;
}

static inline int 
blocking_write(client_t *client, char *data, size_t len)
{
  size_t r = 0, send_len = len;
  while ( (int)len > 0 ){
    if (len < send_len){
      send_len = len;
    }
    r = write(client->fd, data, send_len);
    switch(r){
    case 0:
      return 1;
      break;
    case -1:
      if (errno == EAGAIN || errno == EWOULDBLOCK) { /* try again later */
	//printf("EAGAIN \n");
	usleep(500); //TODO try again later
	break;
      }else{
	// fatal error
	//close
	rb_raise(rb_eException, "fatal error");
        
	// TODO:
	// raise exception from errno
	/* rb_raise(rb_eIOError); */
	/* write_error_log(__FILE__, __LINE__); */
	client->keep_alive = 0;
	return -1;
      }
    default:
      data += (int)r;
      len -= r;
      client->content_length += r;
    }
  }
  return 1;
}

void
send_error_page(client_t *client)
{
  shutdown(client->fd, SHUT_RD);
  if(client->header_done){
    //already sended response data
    //close connection
    return;
  }
  int status = client->bad_request_code;
  int r = status < 0 ? status * -1 : status;
  client->status_code = r;
  switch(r){
  case 400:
    blocking_write(client, MSG_400, sizeof(MSG_400) -1);
    break;
  case 411:
    blocking_write(client, MSG_411, sizeof(MSG_411) -1);
    break;
  case 413:
    blocking_write(client, MSG_413, sizeof(MSG_413) -1);
    break;
  default:
    //Internal Server Error
    blocking_write(client, MSG_500, sizeof(MSG_500) -1);
    break;
  }
  client->keep_alive = 0;
}

static inline void
extent_sndbuf(client_t *client)
{
  int bufsize = 1024 * 1024 * 2, r;
  r = setsockopt(client->fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
  assert(r == 0);
}

static inline void 
enable_cork(client_t *client)
{
  int on = 1, r;
  r = setsockopt(client->fd, IPPROTO_TCP, TCP_CORK, &on, sizeof(on));
  assert(r == 0);
}

static inline write_bucket *
new_write_bucket(int fd, int cnt)
{
  write_bucket *bucket;
  bucket = ruby_xmalloc(sizeof(write_bucket));
  memset(bucket, 0, sizeof(write_bucket));

  bucket->fd = fd;
  bucket->iov = (iovec_t *)ruby_xmalloc(sizeof(iovec_t) * cnt);
  bucket->iov_size = cnt;
  return bucket;
}

static inline void
free_write_bucket(write_bucket *bucket)
{
  ruby_xfree(bucket->iov);
  ruby_xfree(bucket);
}

static inline void
set2bucket(write_bucket *bucket, char *buf, size_t len)
{
  bucket->iov[bucket->iov_cnt].iov_base = buf;
  bucket->iov[bucket->iov_cnt].iov_len = len;
  bucket->iov_cnt++;
  bucket->total += len;
  bucket->total_size += len;
}

static inline void
add_header(write_bucket *bucket, char *key, size_t keylen, char *val, size_t vallen)
{
  set2bucket(bucket, key, keylen);
  set2bucket(bucket, DELIM, 2);
  set2bucket(bucket, val, vallen);
  set2bucket(bucket, CRLF, 2);
}

static inline int 
writev_bucket(write_bucket *data)
{
  size_t w;
  int i = 0;
  w = writev(data->fd, data->iov, data->iov_cnt);
  if(w == -1){
    //error
    if (errno == EAGAIN || errno == EWOULDBLOCK) { 
      // try again later
      return 0;
    }else{
      //ERROR
      rb_raise(rb_eException, "fatal error");

      // TODO:
      // raise exception from errno
      /* rb_raise(rb_eIOError); */
      /* write_error_log(__FILE__, __LINE__);  */
      return -1;
    }
  }if(w == 0){
    return 1;
  }else{
    if(data->total > w){
      for(; i < data->iov_cnt;i++){
	if(w > data->iov[i].iov_len){
	  //already write
	  w -= data->iov[i].iov_len;
	  data->iov[i].iov_len = 0;
	}else{
	  data->iov[i].iov_base += w;
	  data->iov[i].iov_len = data->iov[i].iov_len - w;
	  break;
	}
      }
      data->total = data->total -w;
#ifdef DEBUG
      printf("writev_bucket write %d progeress %d/%d \n", w, data->total, data->total_size);
#endif
      //resume
      // again later
      return writev_bucket(data);
    }
  }
  return 1;
}

static inline int
write_headers(client_t *client)
{
  if(client->header_done){
    return 1;
  }
  write_bucket *bucket;
  uint32_t i = 0, hlen = 0;

  VALUE arr = NULL;
  VALUE object = NULL;
  char *name = NULL;
  ssize_t namelen;
  char *value = NULL;
  long valuelen;

  if(client->headers){
    if (TYPE(client->headers) != T_HASH) {
      return -1;
    }
    arr = rb_funcall(client->headers, i_keys, 0);
    hlen = RARRAY_LEN(arr);
  }
  bucket = new_write_bucket(client->fd, ( hlen * 4 * 2) + 32 );
  
  object = client->http_status;

  if(TYPE(object) != T_STRING){
    return -1;
  }

  if(object){
    value = StringValuePtr(object);
    valuelen = RSTRING_LEN(object);
    //write status code
    set2bucket(bucket, value, valuelen);

    add_header(bucket, "Server", 6,  SERVER, sizeof(SERVER) -1);
    cache_time_update();
    add_header(bucket, "Date", 4, (char *)http_time, 29);
    if(client->keep_alive == 1){
      // Keep-Alive
      add_header(bucket, "Connection", 10, "Keep-Alive", 10);
    } else {
      add_header(bucket, "Connection", 10, "close", 5);
    }
  }

  VALUE object1, object2;
  
  //write header
  if(client->headers){
    for(i=0; i < hlen; i++){
      object1 = rb_ary_entry(arr, i);
      Check_Type(object1, T_STRING);

      if (TYPE(client->headers)!=T_HASH){
	goto error;
      }
      VALUE tmp = rb_funcall(client->headers, i_key, 1, object1);
      if (tmp == Qfalse){
	goto error;
      }
      object2 = rb_hash_aref(client->headers, object1);

      Check_Type(object2, T_STRING);
      
      name = StringValuePtr(object1);
      namelen = RSTRING_LEN(object1);
  
      value = StringValuePtr(object2);
      valuelen = RSTRING_LEN(object2);

      if (strchr(name, '\n') != 0 || strchr(value, '\n') != 0) {
	rb_raise(rb_eArgError, "embedded newline in response header and value");
      }
      
      if (!strcasecmp(name, "Server") || !strcasecmp(name, "Date")) {
 	continue;
      }
      
      if (!strcasecmp(name, "Content-Length")) {
	char *v = value;
	long l = 0;
	
	errno = 0;
	l = strtol(v, &v, 10);
	if (*v || errno == ERANGE || l < 0) {
	  rb_raise(rb_eArgError, "invalid content length");
	  goto error;
	}
	client->content_length_set = 1;
	client->content_length = l;
      }
    add_header(bucket, name, namelen, value, valuelen);
    }
  }
  set2bucket(bucket, CRLF, 2);

  client->bucket = bucket;
  int ret = writev_bucket(bucket);
  if(ret != 0){
    client->header_done = 1;
    // clear
    free_write_bucket(bucket);
    client->bucket = NULL;
  }
  return ret;
 error:
  /* write_error_log(__FILE__, __LINE__); */
  if(bucket){
    free_write_bucket(bucket);
    client->bucket = NULL;
  }
  return -1;
}
  
/* static inline int */
/* write_sendfile(int out_fd, int in_fd, size_t count) */
/* { */
/*   int size = (int)count; */
  
/*   if (size == 0) { */
/*     struct stat info; */
/*     if (fstat(in_fd, &info) == -1){ */
/*       // TODO: */
/*       // raise exception from errno */
/*       /\* rb_raise(); *\/ */
/*       /\* write_error_log(__FILE__, __LINE__);  *\/ */
/*       return -1; */
/*     } */
/*     size = info.st_size - lseek(in_fd, 0, SEEK_CUR); */
/*   } */
/*   return sendfile(out_fd, in_fd, NULL, count); */
/* } */

static inline void
close_response(client_t *client)
{
  //send all response
  //closing reponse object
  client->response_closed = 1;
}

static inline int
processs_write(client_t *client)
{
  VALUE iterator = NULL;
  VALUE arr;
  VALUE item;
  char *buf;
  ssize_t buflen;
  write_bucket *bucket;
  int ret;

  // body
  iterator = client->response_iter;

  if(iterator != NULL){
    if (TYPE(iterator) != T_ARRAY || RARRAY_LEN(iterator) != 3){
      return -1;
    }

    arr = rb_ary_entry(iterator, 2);

    if (TYPE(arr) != T_ARRAY){
      return -1;
    }

    int hlen = RARRAY_LEN(arr);

    for(int i=0; i<hlen;i++){
      item = rb_ary_entry(arr, i);
      if(TYPE(item) != T_STRING) {
	return -1;
      }

      buf = StringValuePtr(item);
      buflen = RSTRING_LEN(item);
      //write
      bucket = new_write_bucket(client->fd, 1);
      set2bucket(bucket, buf, buflen);
      ret = writev_bucket(bucket);
      if(ret <= 0){
	return ret;
      }
      //mark
      client->write_bytes += buflen;
      //check write_bytes/content_length
      if(client->content_length_set){
	if(client->content_length <= client->write_bytes){
	  // all done
	  break;
	}
      }
    }
    close_response(client);
  }
  return 1;
}

inline int
process_body(client_t *client)
{
  int ret;
  write_bucket *bucket;
  if(client->bucket){
    bucket = (write_bucket *)client->bucket;
    //retry send
    ret = writev_bucket(bucket);
    
    if(ret != 0){
      client->write_bytes += bucket->total_size;
      //free
      free_write_bucket(bucket);
      client->bucket = NULL;
    }else{
      return 0;
    }
  }
  ret = processs_write(client);
  return ret;
}

static inline int
start_response_write(client_t *client)
{
  VALUE iterator;
  VALUE dict;
  VALUE item;
  char *buf;
  ssize_t buflen;
    
  iterator = client->response;
  client->response_iter = iterator;

  if (TYPE(iterator) != T_ARRAY){
    return -1;
  }
  assert(3 == RARRAY_LEN(iterator));

  dict = rb_ary_entry(iterator, 1);

  if (TYPE(dict) != T_HASH){
    return -1;
  }

#ifdef DEBUG
  printf("start_response_write buflen %d \n", buflen);
#endif
  return write_headers(client);
}

inline int
response_start(client_t *client)
{
  int ret;
  enable_cork(client);
  if(client->status_code == 304){
    return write_headers(client);
  }
  ret = start_response_write(client);
#ifdef DEBUG
  printf("start_response_write ret = %d \n", ret);
#endif
  if(ret > 0){
    // sended header
    ret = processs_write(client);
  }
  return ret;
}

inline request *
new_request(void)
{
  request *req = (request *)ruby_xmalloc(sizeof(request));
  memset(req, 0, sizeof(request));
  return req;
}

inline header *
new_header(size_t fsize, size_t flimit, size_t vsize, size_t vlimit)
{
  header *h;
  h = ruby_xmalloc(sizeof(header));
  h->field = new_buffer(fsize, flimit);
  h->value = new_buffer(vsize, vlimit);
  return h;
}

inline void
free_header(header *h)
{
  ruby_xfree(h);
}

inline void
free_request(request *req)
{
  uint32_t i;
  header *h;
  if(req->path){
    free_buffer(req->path);
    req->path = NULL;
  }
  if(req->uri){
    free_buffer(req->uri); 
    req->uri = NULL;
  }
  if(req->query_string){
    free_buffer(req->query_string); 
    req->query_string = NULL;
  }
  if(req->fragment){
    free_buffer(req->fragment); 
    req->fragment = NULL;
  }
  for(i = 0; i < req->num_headers+1; i++){
    h = req->headers[i];
    if(h){
      free_buffer(h->field);
      free_buffer(h->value);
      free_header(h);
      req->headers[i] = NULL;
    }
  }
  ruby_xfree(req);
}

static inline void
key_upper(char *s, const char *key, size_t len)
{
  int i = 0;
  register int c;
  for (i = 0; i < len; i++) {
    c = key[i];
    if(c == '-'){
      s[i] = '_';
    }else{
      if(islower(c)){
	s[i] = toupper(c);
      }else{
	s[i] = c;
      }
    }
  }
}

static inline int
write_body2file(client_t *client, const char *buffer, size_t buffer_len)
{
  FILE *tmp = (FILE *)client->body;
  fwrite(buffer, 1, buffer_len, tmp);
  client->body_readed += buffer_len;
#ifdef DEBUG
  printf("write_body2file %d bytes \n", buffer_len);
#endif
  return client->body_readed;
}

static inline int
write_body2mem(client_t *client, const char *buffer, size_t buffer_len)
{
  printf("body2mem called\n");
  VALUE obj = (VALUE)client->body;
  rb_str_concat(obj, rb_str_new(buffer, buffer_len));
  client->body_readed += buffer_len;
#ifdef DEBUG
  printf("write_body2mem %d bytes \n", buffer_len);
#endif
  return client->body_readed;
}

static inline int
write_body(client_t *cli, const char *buffer, size_t buffer_len)
{
  return write_body2mem(cli, buffer, buffer_len);
}

typedef enum{
  CONTENT_TYPE,
  CONTENT_LENGTH,
  OTHER
} rack_header_type;

static inline rack_header_type
check_header_type(const char *buf)
{
  if(*buf++ != 'C'){
    return OTHER;
  }
  if(*buf++ != 'O'){
    return OTHER;
  }
  if(*buf++ != 'N'){
    return OTHER;
  }
  if(*buf++ != 'T'){
    return OTHER;
  }
  if(*buf++ != 'E'){
    return OTHER;
  }
  if(*buf++ != 'N'){
    return OTHER;
  }
  if(*buf++ != 'T'){
    return OTHER;
  }
  if(*buf++ != '_'){
    return OTHER;
  }
  char c = *buf++;
  if(c == 'L'){
    return CONTENT_LENGTH;
  }else if(c == 'T'){
    return CONTENT_TYPE;
  }
  return OTHER;
}

static inline client_t *
get_client(http_parser *p)
{
  return (client_t *)p->data;
}

int
message_begin_cb(http_parser *p)
{
  return 0;
}

int
header_field_cb (http_parser *p, const char *buf, size_t len, char partial)
{
  uint32_t i;
  register header *h;
  client_t *client = get_client(p);
  register request *req = client->req;
  char temp[len];
  
  buffer_result ret = MEMORY_ERROR;
  if (req->last_header_element != FIELD){
    if(LIMIT_REQUEST_FIELDS <= req->num_headers){
      client->bad_request_code = 400;
      return -1;
    }
    req->num_headers++;
  }
  i = req->num_headers;
  h = req->headers[i];
  
  key_upper(temp, buf, len);
  if(h){
    ret = write2buf(h->field, temp, len);
  }else{
    req->headers[i] = h = new_header(128, LIMIT_REQUEST_FIELD_SIZE, 1024, LIMIT_REQUEST_FIELD_SIZE);
    rack_header_type type = check_header_type(temp);
    if(type == OTHER){
      ret = write2buf(h->field, "HTTP_", 5);
    }
    ret = write2buf(h->field, temp, len);
    //printf("%s \n", getString(h->field));
  }
  switch(ret){
  case MEMORY_ERROR:
    client->bad_request_code = 500;
    return -1;
  case LIMIT_OVER:
    client->bad_request_code = 400;
    return -1;
  default:
    break;
  }
  req->last_header_element = FIELD;
  return 0;
}

int
header_value_cb (http_parser *p, const char *buf, size_t len, char partial)
{
  uint32_t i;
  register header *h;
  client_t *client = get_client(p);
  register request *req = client->req;
    
  buffer_result ret = MEMORY_ERROR;
  i = req->num_headers;
  h = req->headers[i];
  
  if(h){
    ret = write2buf(h->value, buf, len);
  }
  switch(ret){
  case MEMORY_ERROR:
    client->bad_request_code = 500;
    return -1;
  case LIMIT_OVER:
    client->bad_request_code = 400;
    return -1;
  default:
    break;
  }
  req->last_header_element = VAL;
  return 0;
}

int
request_path_cb (http_parser *p, const char *buf, size_t len, char partial)
{
  client_t *client = get_client(p);
  register request *req = client->req;
  buffer_result ret = MEMORY_ERROR;
  
  if(req->path){
    ret = write2buf(req->path, buf, len);
  }else{
    req->path = new_buffer(1024, LIMIT_PATH);
    ret = write2buf(req->path, buf, len);
  }
  switch(ret){
  case MEMORY_ERROR:
    client->bad_request_code = 500;
    return -1;
  case LIMIT_OVER:
    client->bad_request_code = 400;
    return -1;
  default:
    break;
  }
  return 0;
}

int
request_uri_cb (http_parser *p, const char *buf, size_t len, char partial)
{
  client_t *client = get_client(p);
  register request *req = client->req;
  buffer_result ret = MEMORY_ERROR;
    
  if(req->uri){
    ret = write2buf(req->uri, buf, len);
  }else{
    req->uri = new_buffer(1024, LIMIT_URI);
    ret = write2buf(req->uri, buf, len);
  }
  switch(ret){
  case MEMORY_ERROR:
    client->bad_request_code = 500;
    return -1;
  case LIMIT_OVER:
    client->bad_request_code = 400;
    return -1;
  default:
    break;
  }
  return 0;
}

int
query_string_cb (http_parser *p, const char *buf, size_t len, char partial)
{
  client_t *client = get_client(p);
  register request *req = client->req;
  buffer_result ret = MEMORY_ERROR;

  if(req->query_string){
    ret = write2buf(req->query_string, buf, len);
  }else{
    req->query_string = new_buffer(1024*2, LIMIT_QUERY_STRING);
    ret = write2buf(req->query_string, buf, len);
  }
  switch(ret){
  case MEMORY_ERROR:
    client->bad_request_code = 500;
    return -1;
  case LIMIT_OVER:
    client->bad_request_code = 400;
    return -1;
  default:
    break;
  }
  return 0;
}

int
fragment_cb (http_parser *p, const char *buf, size_t len, char partial)
{
  client_t *client = get_client(p);
  register request *req = client->req;
  buffer_result ret = MEMORY_ERROR;
  
  if(req->fragment){
    ret = write2buf(req->fragment, buf, len);
  }else{
    req->fragment = new_buffer(1024, LIMIT_FRAGMENT);
    ret = write2buf(req->fragment, buf, len);
  }
  switch(ret){
  case MEMORY_ERROR:
    client->bad_request_code = 500;
    return -1;
  case LIMIT_OVER:
    client->bad_request_code = 400;
    return -1;
  default:
    break;
  }
  return 0;
}

int
body_cb (http_parser *p, const char *buf, size_t len, char partial)
{
  client_t *client = get_client(p);
  if(max_content_length <= client->body_readed + len){
    client->bad_request_code = 413;
    return -1;
  }
  if(client->body_type == BODY_TYPE_NONE){
    if(client->body_length == 0){
      //Length Required
      client->bad_request_code = 411;
      return -1;
    }
    //default memory stream
#ifdef DEBUG
    printf("client->body_length %d \n", client->body_length);
#endif
    client->body_type = BODY_TYPE_BUFFER;
#ifdef DEBUG
    printf("BODY_TYPE_BUFFER \n");
#endif
  }
  write_body(client, buf, len);
  return 0;
}

int
headers_complete_cb(http_parser *p)
{
  register VALUE obj, key;
  client_t *client = get_client(p);
  request *req = client->req;
  register VALUE env = client->environ;
  register uint32_t i = 0; 
  register header *h;
  
  if(max_content_length < p->content_length){
    client->bad_request_code = 413;
    return -1;
  }

  if (p->http_minor == 1) {
    obj = rb_str_new2("HTTP/1.1");
  } else {
    obj = rb_str_new2("HTTP/1.0");
  }    
  rb_hash_aset(env, server_protocol, obj);
    
  if(req->path){
    obj = getRbString(req->path);
    rb_hash_aset(env, path_info, obj);
    req->path = NULL;
  }
  if(req->uri){
    obj = getRbString(req->uri);
    rb_hash_aset(env, request_uri, obj);
    req->uri = NULL;
  }
  if(req->query_string){
    obj = getRbString(req->query_string);
    rb_hash_aset(env, query_string, obj);
    req->query_string = NULL;
  }
  if(req->fragment){
    obj = getRbString(req->fragment);
    rb_hash_aset(env, http_fragment, obj);
    req->fragment = NULL;
  }
  for(i = 0; i < req->num_headers+1; i++){
    h = req->headers[i];
    if(h){
      key = getRbString(h->field);
      obj = getRbString(h->value);
      rb_hash_aset(env, key, obj);
      free_header(h);
      req->headers[i] = NULL;
    }
  }
     
  switch(p->method){
  case HTTP_DELETE:
    obj = rb_str_new("DELETE", 6);
    break;
  case HTTP_GET:
    obj = rb_str_new("GET", 3);
    break;
  case HTTP_HEAD:
    obj = rb_str_new("HEAD", 4);
    break;
  case HTTP_POST:
    obj = rb_str_new("POST", 4);
    break;
  case HTTP_PUT:
    obj = rb_str_new("PUT", 3);
    break;
  case HTTP_CONNECT:
    obj = rb_str_new("CONNECT", 7);
    break;
  case HTTP_OPTIONS:
    obj = rb_str_new("OPTIONS", 7);
    break;
  case  HTTP_TRACE:
    obj = rb_str_new("TRACE", 5);
    break;
  case HTTP_COPY:
    obj = rb_str_new("COPY", 4);
    break;
  case HTTP_LOCK:
    obj = rb_str_new("LOCK", 4);
    break;
  case HTTP_MKCOL:
    obj = rb_str_new("MKCOL", 5);
    break;
  case HTTP_MOVE:
    obj = rb_str_new("MOVE", 4);
    break;
  case HTTP_PROPFIND:
    obj = rb_str_new("PROPFIND", 8);
    break;
  case HTTP_PROPPATCH:
    obj = rb_str_new("PROPPATCH", 9);
    break;
  case HTTP_UNLOCK:
    obj = rb_str_new("UNLOCK", 6);
    break;
  case HTTP_REPORT:
    obj = rb_str_new("REPORT", 6);
    break;
  case HTTP_MKACTIVITY:
    obj = rb_str_new("MKACTIVITY", 10);
    break;
  case HTTP_CHECKOUT:
    obj = rb_str_new("CHECKOUT", 8);
    break;
  case HTTP_MERGE:
    obj = rb_str_new("MERGE", 5);
    break;
  default:
    obj = rb_str_new("GET", 3);
    break;
  }
    
  rb_hash_aset(env, request_method, obj);

  ruby_xfree(req);
  client->req = NULL;
  client->body_length = p->content_length;
  return 0;
}

int
message_complete_cb (http_parser *p)
{
  client_t *client = get_client(p);
  client->complete = 1;
  return 0;
}

static http_parser_settings settings =
  {.on_message_begin = message_begin_cb
  ,.on_header_field = header_field_cb
  ,.on_header_value = header_value_cb
  ,.on_path = request_path_cb
  ,.on_url = request_uri_cb
  ,.on_fragment = fragment_cb
  ,.on_query_string = query_string_cb
  ,.on_body = body_cb
  ,.on_headers_complete = headers_complete_cb
  ,.on_message_complete = message_complete_cb
  };

inline int
init_parser(client_t *cli, const char *name, const short port)
{
  register VALUE object;

  cli->http = (http_parser*)ruby_xmalloc(sizeof(http_parser));
  memset(cli->http, 0, sizeof(http_parser));
    
  cli->environ = rb_hash_new();

  if (cli->environ == NULL) {
    return -1;
  }
  
  rb_hash_aset(cli->environ, version_key, version_val);
  rb_hash_aset(cli->environ, scheme_key, scheme_val);
  rb_hash_aset(cli->environ, errors_key, errors_val);
  rb_hash_aset(cli->environ, multithread_key, multithread_val);
  rb_hash_aset(cli->environ, multiprocess_key, multiprocess_val);
  rb_hash_aset(cli->environ, run_once_key, run_once_val);
  rb_hash_aset(cli->environ, script_key, script_val);
  rb_hash_aset(cli->environ, server_name_key, server_name_val);
  rb_hash_aset(cli->environ, server_port_key, server_port_val);

  // query_string
  rb_hash_aset(cli->environ, query_string, empty_string);

  object = rb_str_new2(cli->remote_addr);
  rb_hash_aset(cli->environ, rb_remote_addr, object);

  char r_port[6];
  sprintf(r_port, "%d", cli->remote_port);
  object = rb_str_new2(r_port);
  rb_hash_aset(cli->environ, rb_remote_port, object);

  http_parser_init(cli->http, HTTP_REQUEST);
  cli->http->data = cli;

  return 0;
}

inline size_t
execute_parse(client_t *cli, const char *data, size_t len)
{
  return http_parser_execute(cli->http, &settings, data, len);
}

inline int
parser_finish(client_t *cli)
{
  return cli->complete;
}

inline void
setup_static_env(char *name, int port)
{
  version_val = rb_obj_freeze(rb_ary_new3(2, INT2FIX(1), INT2FIX(1)));
  version_key = rb_obj_freeze(rb_str_new2("rack.version"));
  
  scheme_val = rb_obj_freeze(rb_str_new2("http"));
  scheme_key = rb_obj_freeze(rb_str_new2("rack.url_scheme"));

  errors_val = rb_stderr;
  errors_key = rb_obj_freeze(rb_str_new2("rack.errors"));

  multithread_val = Qfalse;
  multithread_key = rb_obj_freeze(rb_str_new2("rack.multithread"));

  multiprocess_val = Qfalse; /* or Qtrue? I have no clue.. */ 
  multiprocess_key = rb_obj_freeze(rb_str_new2("rack.multiprocess"));

  run_once_val = Qfalse;
  run_once_key = rb_obj_freeze(rb_str_new2("rack.run_once"));

  script_val = empty_string;
  script_key = rb_obj_freeze(rb_str_new2("SCRIPT_NAME"));

  server_name_val = rb_obj_freeze(rb_str_new2(name));
  server_name_key = rb_obj_freeze(rb_str_new2("SERVER_NAME"));
  
  char vport[6];
  sprintf(vport, "%d", port);
  server_port_val = rb_obj_freeze(rb_str_new2(vport));
  server_port_key = rb_obj_freeze(rb_str_new2("SERVER_PORT"));

  server_protocol = rb_obj_freeze(rb_str_new2("SERVER_PROTOCOL"));
  path_info = rb_obj_freeze(rb_str_new2("PATH_INFO"));
  request_uri = rb_obj_freeze(rb_str_new2("REQUEST_URI"));
  query_string = rb_obj_freeze(rb_str_new2("QUERY_STRING"));
  http_fragment = rb_obj_freeze(rb_str_new2("HTTP_FRAGMENT"));
  request_method = rb_obj_freeze(rb_str_new2("REQUEST_METHOD"));
  rb_remote_addr = rb_obj_freeze(rb_str_new2("REMOTE_ADDR"));
  rb_remote_port = rb_obj_freeze(rb_str_new2("REMOTE_PORT"));
  rack_input = rb_obj_freeze(rb_str_new2("rack.input"));
  http_connection = rb_obj_freeze(rb_str_new2("HTTP_CONNECTION"));

  http_user_agent = rb_obj_freeze(rb_str_new2("HTTP_USER_AGENT"));
}

static inline int
setsig(int sig, void* handler)
{
  struct sigaction context, ocontext;
  context.sa_handler = handler;
  sigemptyset(&context.sa_mask);
  context.sa_flags = 0;
  return sigaction(sig, &context, &ocontext);
}

static inline void 
setup_sock(int fd)
{
  int on = 1, r;
  r = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  assert(r == 0);
  r = fcntl(fd, F_SETFL, O_NONBLOCK);
  assert(r == 0);
}

static inline void 
disable_cork(client_t *client)
{
  int off = 0;
  int on = 1, r;
  r = setsockopt(client->fd, IPPROTO_TCP, TCP_CORK, &off, sizeof(off));
  assert(r == 0);

  r = setsockopt(client->fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  assert(r == 0);
}

static inline client_t *
new_client_t(int client_fd, struct sockaddr_in client_addr){
  client_t *client;
    
  client = ruby_xmalloc(sizeof(client_t));
  memset(client, 0, sizeof(client_t));
    
  /* printf("size %d\n", sizeof(client_t)); */

  client->fd = client_fd;
    
  client->remote_addr = inet_ntoa(client_addr.sin_addr);
  client->remote_port = ntohs(client_addr.sin_port);
  client->req = new_request();
  client->body_type = BODY_TYPE_NONE;
  //printf("input_buf_size %d\n", client->input_buf_size);
  return client;
}

static inline void
clean_cli(client_t *client)
{
  write_access_log(client, log_fd, log_path); 
  if(client->req){
    free_request(client->req);
    client->req = NULL;
  }

  if(client->http != NULL){
    ruby_xfree(client->http);
    client->http = NULL;
  }
}

static inline void 
close_conn(client_t *cli, picoev_loop* loop)
{
  if(!cli->keep_alive){
    picoev_del(loop, cli->fd);
    clean_cli(cli);
    close(cli->fd);
#ifdef DEBUG
    printf("close fd %d \n", cli->fd);
#endif
    ruby_xfree(cli);
  }else{
    clean_cli(cli);
    disable_cork(cli);
    cli->keep_alive = 1;
    cli->environ = NULL;
    cli->http_status = NULL;
    cli->headers = NULL;
    cli->header_done = 0;
    cli->body_type = BODY_TYPE_NONE;
    cli->status_code = 0;
    cli->response = NULL;
    cli->content_length_set = 0;
    cli->content_length = 0;
    cli->write_bytes = 0;
    cli->response_closed = 0;
    cli->bad_request_code = 0;
    init_parser(cli, server_name, server_port);
  }
}

static inline int
process_rack_app(client_t *cli)
{
  VALUE args = NULL;
  char *status;

  args = cli->environ;

  // cli->response = [200, {}, []]
  cli->response = rb_funcall(rack_app, i_call, 1, args);

  if(TYPE(cli->response) != T_ARRAY || RARRAY_LEN(cli->response) < 3) {
    return 0;
  }
  
  VALUE* response_ary = RARRAY_PTR(cli->response);

  if (TYPE(response_ary[0])!=T_FIXNUM ||
      TYPE(response_ary[1])!=T_HASH   ||
      TYPE(response_ary[2])!=T_ARRAY){
    return 0;
  }

  cli->status_code = NUM2INT(response_ary[0]);
  cli->headers = response_ary[1];

  errno = 0;
  /* printf("status code: %d\n", cli->status_code); */

  char buff[256];
  sprintf(buff, "HTTP/1.1 %d\r\n", cli->status_code);
  cli->http_status = rb_str_new(buff, strlen(buff));

  //check response
  if(cli->response && cli->response == Qnil){
    /*   write_error_log(__FILE__, __LINE__); */
    rb_raise(rb_eException, "response must be a iter or sequence object");
  }

  return 1;
}

static void
w_callback(picoev_loop* loop, int fd, int events, void* cb_arg)
{
  client_t *client = ( client_t *)(cb_arg);
  int ret;
#ifdef DEBUG
  printf("call w_callback \n");
#endif
  if ((events & PICOEV_TIMEOUT) != 0) {
#ifdef DEBUG
    printf("** w_callback timeout ** \n");
#endif
    //timeout
    client->keep_alive = 0;
    close_conn(client, loop);

  } else if ((events & PICOEV_WRITE) != 0) {
    ret = process_body(client);
    picoev_set_timeout(loop, client->fd, WRITE_TIMEOUT_SECS);
#ifdef DEBUG
    printf("process_body ret %d \n", ret);
#endif
    if(ret != 0){
      //ok or die
      close_conn(client, loop);
    }
  }
}

static inline void
call_rack_app(client_t *client, picoev_loop* loop)
{
  int ret;
  if(!process_rack_app(client)){
    //Internal Server Error
    client->bad_request_code = 500;
    send_error_page(client);
    close_conn(client, loop);
    return;
  }

  ret = response_start(client);
  /* printf("response_start done: %d\n", ret); */
  switch(ret){
  case -1:
    // Internal Server Error
    client->bad_request_code = 500;
    send_error_page(client);
    close_conn(client, loop);
    return;
  case 0:
    // continue
    // set callback
#ifdef DEBUG
    printf("set write callback %d \n", ret);
#endif
    picoev_add(loop, client->fd, PICOEV_WRITE, WRITE_TIMEOUT_SECS, w_callback, (void *)client);
    return;
  default:
    // send OK
    close_conn(client, loop);
  }
}

static inline void
prepare_call_rack(client_t *client)
{
  VALUE input = NULL, object = NULL, c = NULL;
  char *val;

  object = rb_str_new2("");
  input = rb_funcall(StringIO, i_new, 1, object);
  rb_hash_aset((VALUE)client->environ, rack_input, input);
  client->body = object;

  if(is_keep_alive){
    //support keep-alive
    c = rb_hash_aref(client->environ, http_connection);
    if(c){
      val = StringValuePtr(c);
      if(!strcasecmp(val, "keep-alive")){
	client->keep_alive = 1;
      }else{
	client->keep_alive = 0;
      }
    }else{
      client->keep_alive = 0;
    }
  }
}

static void
r_callback(picoev_loop* loop, int fd, int events, void* cb_arg)
{
  client_t *cli = ( client_t *)(cb_arg);
  if ((events & PICOEV_TIMEOUT) != 0) {
#ifdef DEBUG
    printf("** r_callback timeout ** \n");
#endif
    //timeout
    cli->keep_alive = 0;
    close_conn(cli, loop);
  } else if ((events & PICOEV_READ) != 0) {
#ifdef DEBUG
    printf("ready read \n");
#endif
    /* update timeout, and read */
    int finish = 0, nread;
    char buf[INPUT_BUF_SIZE];
    ssize_t r;
    if(!cli->keep_alive){
      picoev_set_timeout(loop, cli->fd, SHORT_TIMEOUT_SECS);
    }
    r = read(cli->fd, buf, sizeof(buf));
    switch (r) {
    case 0: 
      finish = 1;
      break;
    case -1: /* error */
      if (errno == EAGAIN || errno == EWOULDBLOCK) { /* try again later */
	break;
      } else { /* fatal error */
	rb_raise(rb_eException, "fatal error");
	// TODO:
	// raise exception from errno
	/* rb_raise(); */
	/* write_error_log(__FILE__, __LINE__);  */
	cli->keep_alive = 0;
	cli->status_code = 500;
	close_conn(cli, loop);
	return;
      }
      break;
    default:
#ifdef DEBUG
      printf("read request fd %d bufsize %d \n", cli->fd, r);
#endif
      nread = execute_parse(cli, buf, r);
                
      if(cli->bad_request_code > 0){
#ifdef DEBUG
	printf("fd %d bad_request code %d \n", cli->fd,  cli->bad_request_code);
#endif
	send_error_page(cli);
	close_conn(cli, loop);
	return;
      }
      if( nread != r ){
	// parse error
#ifdef DEBUG
	printf("fd %d parse error %d \n", cli->fd, cli->bad_request_code);
#endif
	cli->bad_request_code = 400;
	send_error_page(cli);
	close_conn(cli, loop);
	return;
      }
#ifdef DEBUG
      printf("parse ok, fd %d %d nread \n", cli->fd, nread);
#endif
      if(parser_finish(cli) > 0){
	finish = 1;
      }
      break;
    }
    
    if(finish == 1){
      prepare_call_rack(cli);
      call_rack_app(cli, loop);
      return;
    }
  }
}

static void
accept_callback(picoev_loop* loop, int fd, int events, void* cb_arg)
{
  int client_fd;
  client_t *client;
  struct sockaddr_in client_addr;
  if ((events & PICOEV_TIMEOUT) != 0) {
    // time out
    // next turn or other process
    return;
  }else if ((events & PICOEV_READ) != 0) {
    socklen_t client_len = sizeof(client_addr);
    client_fd = accept4(fd, (struct sockaddr *)&client_addr, &client_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd != -1) {
#ifdef DEBUG
      printf("accept fd %d \n", client_fd);
#endif
      setup_sock(client_fd);
      client = new_client_t(client_fd, client_addr);

      client->environ = Qnil;
      rb_gc_register_address(&client->environ);

      init_parser(client, server_name, server_port);

      picoev_add(loop, client_fd, PICOEV_READ, READ_LONG_TIMEOUT_SECS, r_callback, (void *)client);
    }else{
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
	// TODO:
	// raise exception from errno
	/* rb_raise(); */
	/* write_error_log(__FILE__, __LINE__); */
	// die
	loop_done = 0;
      }
    }
  }
}

static inline void
setup_server_env(void)
{
  setup_sock(listen_sock);
  cache_time_init();

  setup_static_env(server_name, server_port);
}

static inline int 
inet_listen(void)
{
  struct addrinfo hints, *servinfo, *p;
  int flag = 1;
  int rv;
  char strport[7];
  
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; 
  
  snprintf(strport, sizeof (strport), "%d", server_port);
  
  if ((rv = getaddrinfo(server_name, strport, &hints, &servinfo)) == -1) {
    // TODO:
    // raise exception from errno
    /* rb_raise(); */
    return -1;
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((listen_sock = socket(p->ai_family, p->ai_socktype,
			      p->ai_protocol)) == -1) {
      //perror("server: socket");
      continue;
    }
    
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &flag,
		   sizeof(int)) == -1) {
      close(listen_sock);
      // TODO:
      // raise exception from errno
      /* rb_raise(); */
      return -1;
    }

    if (bind(listen_sock, p->ai_addr, p->ai_addrlen) == -1) {
      close(listen_sock);
      // TODO:
      // raise exception from errno
      /* rb_raise(); */
      return -1;
    }
    break;
  }

  if (p == NULL)  {
    close(listen_sock);
    rb_raise(rb_eIOError, "server: failed to bind\n");
    return -1;
  }

  freeaddrinfo(servinfo); // all done with this structure
    
  // BACKLOG 1024
  if (listen(listen_sock, BACKLOG) == -1) {
    close(listen_sock);
    // TODO:
    // raise exception from errno
    /* rb_raise(); */
    return -1;
  }
  setup_server_env();
  return 1;
}

static void 
sigint_cb(int signum)
{
  loop_done = 0;
  /* rb_interrupt(); */
}

static void 
sigpipe_cb(int signum)
{
}

static VALUE
bossan_stop(VALUE self)
{
  loop_done = 0;
  return Qnil;
}

static VALUE
bossan_access_log(VALUE self, VALUE args)
{   
  log_path = StringValuePtr(args);

  if(log_fd > 0){
    close(log_fd);
  }
  log_fd = open_log_file(log_path);

  if(log_fd < 0){
    rb_raise(rb_eTypeError, "not open file. %s", log_path);
  }
  return Qnil;
}

// Bossan.run('127.0.0.1', 8000, proc do |env|
//   ...
// end
static VALUE
bossan_run_loop(VALUE self, VALUE args1, VALUE args2, VALUE args3)
{
  int ret;

  if(listen_sock > 0){
    rb_raise(rb_eException, "already set listen socket");
  }
  
  server_name = StringValuePtr(args1);
  server_port = NUM2INT(args2);

  long _port = NUM2INT(args2);

  if (_port <= 0 || _port >= 65536) {
    // out of range
    rb_raise(rb_eArgError, "port number outside valid range");
  }

  server_port = (short)_port;

  ret = inet_listen();

  if(ret < 0){
    //error
    listen_sock = -1;
  }

  rack_app = args3;

  if(listen_sock <= 0){
    rb_raise(rb_eTypeError, "not found listen socket");
  }
    
  /* init picoev */
  picoev_init(MAX_FDS);
  /* create loop */
  main_loop = picoev_create_loop(60);
  loop_done = 1;
  
  setsig(SIGPIPE, sigpipe_cb);
  setsig(SIGINT, sigint_cb);
  setsig(SIGTERM, sigint_cb);
    
  picoev_add(main_loop, listen_sock, PICOEV_READ, ACCEPT_TIMEOUT_SECS, accept_callback, NULL);
    
  /* loop */
  while (loop_done) {
    picoev_loop_once(main_loop, 10);
  }
  picoev_destroy_loop(main_loop);
  picoev_deinit();

  printf("Bye.\n");
  return Qnil;
}

VALUE 
bossan_set_max_content_length(VALUE self, VALUE args)
{
  max_content_length = NUM2INT(args);
  return Qnil;
}

VALUE 
bossan_get_max_content_length(VALUE self)
{
  return INT2NUM(max_content_length);
}

void
Init_bossan_ext(void)
{
  rb_gc_register_address(&version_key);
  rb_gc_register_address(&version_val);
  rb_gc_register_address(&scheme_key);
  rb_gc_register_address(&scheme_val);
  rb_gc_register_address(&errors_key);
  rb_gc_register_address(&errors_val);
  rb_gc_register_address(&multithread_key);
  rb_gc_register_address(&multithread_val);
  rb_gc_register_address(&multiprocess_key);
  rb_gc_register_address(&multiprocess_val);
  rb_gc_register_address(&run_once_key);
  rb_gc_register_address(&run_once_val);

  rb_gc_register_address(&script_key);
  rb_gc_register_address(&script_val);
  rb_gc_register_address(&server_name_key);
  rb_gc_register_address(&server_name_val);
  rb_gc_register_address(&server_port_key);
  rb_gc_register_address(&server_port_val);

  rb_gc_register_address(&server_protocol);
  rb_gc_register_address(&path_info);
  rb_gc_register_address(&request_uri);
  rb_gc_register_address(&query_string);
  rb_gc_register_address(&http_fragment);
  rb_gc_register_address(&request_method);
  rb_gc_register_address(&rb_remote_addr);
  rb_gc_register_address(&rb_remote_port);
  rb_gc_register_address(&rack_input);
  rb_gc_register_address(&http_connection);

  rb_gc_register_address(&http_user_agent);

  empty_string = rb_obj_freeze(rb_str_new2(""));
  rb_gc_register_address(&empty_string);

  rb_gc_register_address(&i_keys);
  rb_gc_register_address(&i_call);
  rb_gc_register_address(&i_new);
  rb_gc_register_address(&i_key);

  rb_gc_register_address(&rack_app); //rack app

  i_new = rb_intern("new");
  i_call = rb_intern("call");
  i_keys = rb_intern("keys");
  i_key = rb_intern("key?");

  server = rb_define_module("Bossan");
  rb_gc_register_address(&server);

  rb_define_module_function(server, "run", bossan_run_loop, 3);
  rb_define_module_function(server, "stop", bossan_stop, 0);

  rb_define_module_function(server, "access_log", bossan_access_log, 1);
  rb_define_module_function(server, "set_max_content_length", bossan_set_max_content_length, 1);
  rb_define_module_function(server, "get_max_content_length", bossan_get_max_content_length, 0);

  rb_require("stringio");
  StringIO = rb_const_get(rb_cObject, rb_intern("StringIO"));
}
