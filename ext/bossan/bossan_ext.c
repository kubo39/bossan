#include "bossan.h"

#include "time_cache.h"
#include "http_parser.h"
#include "picoev.h"
#include "buffer.h"
#include "client.h"


#define ACCEPT_TIMEOUT_SECS 1
#define SHORT_TIMEOUT_SECS 2
#define WRITE_TIMEOUT_SECS 5
#define READ_LONG_TIMEOUT_SECS 5

#define MAX_BUFSIZE 1024 * 8
#define INPUT_BUF_SIZE 1024 * 8

#define LIMIT_SIZE 1024 * 512

#define READ_TIMEOUT_SECS 30
#define READ_BUF_SIZE 1024 * 64

#define CRLF "\r\n"
#define DELIM ": "

#define SERVER "bossan/0.4.1"

#define H_MSG_500 "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\nServer:  " SERVER "\r\n\r\n"

#define H_MSG_503 "HTTP/1.0 503 Service Unavailable\r\nContent-Type: text/html\r\nServer: " SERVER "\r\n\r\n"

#define H_MSG_400 "HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\nServer: " SERVER "\r\n\r\n"

#define H_MSG_408 "HTTP/1.0 408 Request Timeout\r\nContent-Type: text/html\r\nServer: " SERVER "\r\n\r\n"

#define H_MSG_411 "HTTP/1.0 411 Length Required\r\nContent-Type: text/html\r\nServer: " SERVER "\r\n\r\n"

#define H_MSG_413 "HTTP/1.0 413 Request Entity Too Large\r\nContent-Type: text/html\r\nServer: " SERVER "\r\n\r\n"

#define H_MSG_417 "HTTP/1.1 417 Expectation Failed\r\nContent-Type: text/html\r\nServer: " SERVER "\r\n\r\n"


#define MSG_500 H_MSG_500 "<html><head><title>500 Internal Server Error</title></head><body><h1>Internal Server Error</h1><p>The server encountered an internal error and was unable to complete your request.  Either the server is overloaded or there is an error in the application.</p></body></html>"

#define MSG_503 H_MSG_503 "<html><head><title>Service Unavailable</title></head><body><p>Service Unavailable.</p></body></html>"

#define MSG_400 H_MSG_400 "<html><head><title>Bad Request</title></head><body><p>Bad Request.</p></body></html>"

#define MSG_408 H_MSG_408 "<html><head><title>Request Timeout</title></head><body><p>Request Timeout.</p></body></html>"

#define MSG_411 H_MSG_411 "<html><head><title>Length Required</title></head><body><p>Length Required.</p></body></html>"

#define MSG_413 H_MSG_413 "<html><head><title>Request Entity Too Large</title></head><body><p>Request Entity Too Large.</p></body></html>"

#define MSG_417 H_MSG_417 "<html><head><title>Expectation Failed</title></head><body><p>Expectation Failed.</p></body></html>"

VALUE server; // Bossan

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

static VALUE content_type;
static VALUE content_length_key;

static VALUE h_content_type;
static VALUE h_content_length;

static VALUE empty_string;

static VALUE http_10;
static VALUE http_11;

static VALUE http_delete;
static VALUE http_get;
static VALUE http_head;
static VALUE http_post;
static VALUE http_put;
static VALUE http_connect;
static VALUE http_options;
static VALUE http_trace;
static VALUE http_copy;
static VALUE http_lock;
static VALUE http_mkcol;
static VALUE http_move;
static VALUE http_propfind;
static VALUE http_proppatch;
static VALUE http_unlock;
static VALUE http_report;
static VALUE http_mkactivity;
static VALUE http_checkout;
static VALUE http_merge;

static VALUE http_user_agent;
static VALUE http_referer;

static VALUE http_expect;

static ID i_keys;
static ID i_call;
static ID i_new;
static ID i_each;
static ID i_toenum;
static ID i_close;
static ID i_write;
static ID i_seek;
static ID i_toa;
static ID i_next;

static VALUE default_path_string;

static const char *server_name = "127.0.0.1";
static short server_port = 8000;

static int prefix_len = 0;

static int listen_sock;  // listen socket

static char *unix_sock_name = NULL;

static int loop_done; // main loop flag
static picoev_loop* main_loop; //main loop
static VALUE rack_app; //rack app

static char *log_path = NULL; //access log path
static int log_fd = -1; //access log

static int is_keep_alive = 0; //keep alive support
static int keep_alive_timeout = 5;

static int max_fd = 1024 * 4;  // picoev max_fd size
static int backlog = 1024;  // backlog size
int max_content_length = 1024 * 1024 * 16; //max_content_length
int client_body_buffer_size = 1024 * 500;  //client_body_buffer_size

static VALUE StringIO;

typedef struct iovec iovec_t;

typedef struct {
  int fd;
  iovec_t *iov;
  uint32_t iov_cnt;
  uint32_t iov_size;
  uint32_t total;
  uint32_t total_size;
  uint8_t sended;
  VALUE temp1; //keep origin pointer
  VALUE chunk_data; //keep chunk_data origin pointer
} write_bucket;


typedef enum {
  STATUS_OK = 0,
  STATUS_SUSPEND,
  STATUS_ERROR 
} response_status;


static void
r_callback(picoev_loop* loop, int fd, int events, void* cb_arg);

static void
w_callback(picoev_loop* loop, int fd, int events, void* cb_arg);

static void
call_rack_app(client_t *client);

static void
prepare_call_rack(client_t *client);

int
open_log_file(const char *path)
{
  return open(path, O_CREAT|O_APPEND|O_WRONLY, 0744);
}


void
write_error_log(char *file_name, int line)
{
  char buf[64];
  FILE *fp = stderr;
  int fd = fileno(fp);

  flock(fd, LOCK_EX);

  cache_time_update();
  fputs((char *)err_log_time, fp);
  fputs(" [error] ", fp);

  sprintf(buf, "pid %d, File \"%s\", line %d :", getpid(), file_name, line);
  fputs(buf, fp);
  fflush(fp);

  flock(fd, LOCK_UN);
}


static int
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


static int
write_access_log(client_t *cli, int log_fd, const char *log_path)
{
  request *req = cli->current_req;

  char buf[1024*4];
  if(log_fd > 0){
    VALUE obj;
    char *method, *path, *version, *ua, *referer;
        
    obj = rb_hash_aref(req->environ, request_method);
    if(obj != Qnil){
      method = StringValuePtr(obj);
    }else{
      method = "-";
    }
                
    obj = rb_hash_aref(req->environ, path_info);
    if(obj != Qnil){
      path = StringValuePtr(obj);
    }else{
      path = "-";
    }
    
    obj = rb_hash_aref(req->environ, server_protocol);
    if(obj != Qnil){
      version = StringValuePtr(obj);
    }else{
      version = "-";
    }

    obj = rb_hash_aref(req->environ, http_user_agent);
    if(obj != Qnil){
      ua = StringValuePtr(obj);
    }else{
      ua = "-";
    }

    obj = rb_hash_aref(req->environ, http_referer);
    if(obj != Qnil){
      referer = StringValuePtr(obj);
    }else{
      referer = "-";
    }

    //update
    cache_time_update();
        
    sprintf(buf, "%s - - [%s] \"%s %s %s\" %d %d \"%s\" \"%s\"\n",
	    cli->remote_addr,
	    http_log_time,
	    method,
	    path,
	    version,
	    cli->status_code,
	    cli->write_bytes,
	    referer,
	    ua);
    return write_log(log_path, log_fd, buf, strlen(buf));
  }
  return 0;
}


static int
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
	usleep(500); //TODO try again later
	break;
      }else{
	// fatal error
	//close

	if(errno == EPIPE){
	  // Connection reset by peer
	  client->keep_alive = 0;
	  client->status_code = 500;
	  client->header_done = 1;
	  client->response_closed = 1;

	}else{
	  // TODO:
	  // raise exception from errno

	  write_error_log(__FILE__, __LINE__);
	  client->keep_alive = 0;
	}
	return -1;
      }
    default:
      data += (int)r;
      len -= r;
      client->write_bytes += r;
    }
  }
  return 1;
}


static void
send_error_page(client_t *client)
{
  shutdown(client->fd, SHUT_RD);
  if(client->header_done || client->response_closed){
    //already sended response data
    //close connection
    return;
  }

  switch(client->status_code){
  case 400:
    blocking_write(client, MSG_400, sizeof(MSG_400) -1);
    client->write_bytes -= sizeof(H_MSG_400) -1;
    break;
  case 408:
    blocking_write(client, MSG_408, sizeof(MSG_408) -1);
    client->write_bytes -= sizeof(H_MSG_408) -1;
    break;
  case 411:
    blocking_write(client, MSG_411, sizeof(MSG_411) -1);
    client->write_bytes -= sizeof(H_MSG_411) -1;
    break;
  case 413:
    blocking_write(client, MSG_413, sizeof(MSG_413) -1);
    client->write_bytes -= sizeof(H_MSG_413) -1;
    break;
  case 417:
    blocking_write(client, MSG_417, sizeof(MSG_417) -1);
    client->write_bytes -= sizeof(H_MSG_417) -1;
    break;
  case 503:
    blocking_write(client, MSG_503, sizeof(MSG_503) -1);
    client->write_bytes -= sizeof(H_MSG_503) -1;
    break;
  default:
    //Internal Server Error
    blocking_write(client, MSG_500, sizeof(MSG_500) -1);
    client->write_bytes -= sizeof(H_MSG_500) -1;
    break;
  }
  client->keep_alive = 0;
  client->header_done = 1;
  client->response_closed = 1;
}


static write_bucket *
new_write_bucket(int fd, int cnt)
{
  write_bucket *bucket;
  iovec_t *iov;

  bucket = ruby_xmalloc(sizeof(write_bucket));
  memset(bucket, 0, sizeof(write_bucket));

  bucket->fd = fd;
  iov = (iovec_t *)ruby_xmalloc(sizeof(iovec_t) * cnt);
  memset(iov, 0, sizeof(iovec_t));
  bucket->iov = iov;
  bucket->iov_size = cnt;
  GDEBUG("allocate %p", bucket);
  return bucket;
}


static void
free_write_bucket(write_bucket *bucket)
{
  GDEBUG("free %p", bucket);
  ruby_xfree(bucket->iov);
  ruby_xfree(bucket);
}


static void
set2bucket(write_bucket *bucket, char *buf, size_t len)
{
  bucket->iov[bucket->iov_cnt].iov_base = buf;
  bucket->iov[bucket->iov_cnt].iov_len = len;
  bucket->iov_cnt++;
  bucket->total += len;
  bucket->total_size += len;
}


static void
set_chunked_data(write_bucket *bucket, char *lendata, size_t lenlen, char *data, size_t datalen)
{
  set2bucket(bucket, lendata, lenlen);
  set2bucket(bucket, CRLF, 2);
  set2bucket(bucket, data, datalen);
  set2bucket(bucket, CRLF, 2);
}


static void
set_last_chunked_data(write_bucket *bucket)
{
  set2bucket(bucket, "0", 1);
  set2bucket(bucket, CRLF, 2);
  set2bucket(bucket, CRLF, 2);
}


static void
add_header(write_bucket *bucket, char *key, size_t keylen, char *val, size_t vallen)
{
  set2bucket(bucket, key, keylen);
  set2bucket(bucket, DELIM, 2);
  set2bucket(bucket, val, vallen);
  set2bucket(bucket, CRLF, 2);
}


#ifdef DEVELOP
static void
writev_log(write_bucket *data)
{
  int i = 0;
  char *c;
  size_t len;
  for(; i < data->iov_cnt; i++){
    c = data->iov[i].iov_base;
    len = data->iov[i].iov_len;
    printf("%.*s", (int)len, c); 
  }
}
#endif


static response_status
writev_bucket(write_bucket *data)
{
  size_t w;
  int i = 0;
#ifdef DEVELOP
  BDEBUG("\nwritev_bucket fd:%d", data->fd);
  printf("\x1B[34m");
  writev_log(data);
  printf("\x1B[0m\n");
#endif
  w = writev(data->fd, data->iov, data->iov_cnt);
  BDEBUG("writev fd:%d ret:%d total_size:%d", data->fd, (int)w, data->total);
  if(w == -1){
    //error
    if (errno == EAGAIN || errno == EWOULDBLOCK) { 
      // try again later
      return STATUS_SUSPEND;
    }else{
      //ERROR

      // TODO:
      /* write_error_log(__FILE__, __LINE__); */
      return STATUS_ERROR;
    }
  }if(w == 0){
    data->sended = 1;
    return STATUS_OK;
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
      data->total = data->total - w;
      DEBUG("writev_bucket write %d progeress %d/%d \n", w, data->total, data->total_size);
      //resume
      // again later
      return writev_bucket(data);
    }
  }
  data->sended = 1;
  return STATUS_OK;
}


static VALUE
get_chunk_data(size_t datalen)
{
  char lendata[32];
  int i = 0;
  i = snprintf(lendata, 32, "%zx", datalen);
  DEBUG("Transfer-Encoding chunk_size %s", lendata);
  return rb_str_new(lendata, i);
}


static void
set_first_body_data(client_t *client, char *data, size_t datalen)
{
  write_bucket *bucket = client->bucket;
  if(data){
    if(client->chunked_response){
      char *lendata  = NULL;
      ssize_t len = 0;

      VALUE chunk_data = get_chunk_data(datalen);
      //TODO CHECK ERROR
      lendata = StringValuePtr(chunk_data);
      len = RSTRING_LEN(chunk_data);
      set_chunked_data(bucket, lendata, len, data, datalen);
      bucket->chunk_data = chunk_data;
    }else{
      set2bucket(bucket, data, datalen);
    }
  }
}


static response_status
write_headers(client_t *client, char *data, size_t datalen)
{
  write_bucket *bucket;
  uint32_t i = 0, hlen = 0;

  VALUE arr;
  VALUE object;
  char *name = NULL;
  ssize_t namelen;
  char *value = NULL;
  long valuelen;
  response_status ret;

  DEBUG("header write? %d", client->header_done);
  if(client->header_done){
    return STATUS_OK;
  }

  if (TYPE(client->headers) != T_HASH) {
    return STATUS_ERROR;
  }

  arr = rb_funcall(client->headers, i_keys, 0);
  hlen = RARRAY_LEN(arr);

  bucket = new_write_bucket(client->fd, ( hlen * 4 * 2) + 32 );
  
  object = client->http_status;

  if(object){
    value = StringValuePtr(object);
    valuelen = RSTRING_LEN(object);
    //write status code
    set2bucket(bucket, value, valuelen);

    add_header(bucket, "Server", 6,  SERVER, sizeof(SERVER) -1);
    add_header(bucket, "Date", 4, (char *)http_time, 29);
  }

  VALUE object1, object2;
  
  //write header
  for(i=0; i < hlen; i++){
    object1 = rb_ary_entry(arr, i);

    object2 = rb_hash_aref(client->headers, object1);
      
    name = StringValuePtr(object1);
    namelen = RSTRING_LEN(object1);
  
    value = StringValuePtr(object2);
    valuelen = RSTRING_LEN(object2);

    if (strchr(name, ':') != 0) {
      goto error;
    }

    if (strchr(name, '\n') != 0 || strchr(value, '\n') != 0) {
      goto error;
    }
      
    if (!strcasecmp(name, "Server") || !strcasecmp(name, "Date")) {
      continue;
    }
      
    if (client->content_length_set != 1 && !strcasecmp(name, "Content-Length")) {
      char *v = value;
      long l = 0;
	
      errno = 0;
      l = strtol(v, &v, 10);
      if (*v || errno == ERANGE || l < 0) {
	goto error;
      }
      client->content_length_set = 1;
      client->content_length = l;
    }
    add_header(bucket, name, namelen, value, valuelen);
  }

  // check content_length_set
  if(data && !client->content_length_set && client->http_parser->http_minor == 1){
    //Transfer-Encoding chunked
    add_header(bucket, "Transfer-Encoding", 17, "chunked", 7);
    client->chunked_response = 1;
  }

  if(client->keep_alive == 1){
    // Keep-Alive
    add_header(bucket, "Connection", 10, "Keep-Alive", 10);
  } else {
    add_header(bucket, "Connection", 10, "close", 5);
  }
  set2bucket(bucket, CRLF, 2);

  // write_body
  client->bucket = bucket;
  set_first_body_data(client, data, datalen);

  ret = writev_bucket(bucket);
  if(ret != STATUS_SUSPEND){
    client->header_done = 1;
    if(ret == STATUS_OK && data){
      client->write_bytes += datalen;
    }
    // clear
    free_write_bucket(bucket);
    client->bucket = NULL;
  }
  return ret;
 error:
  write_error_log(__FILE__, __LINE__);
  if(bucket){
    free_write_bucket(bucket);
    client->bucket = NULL;
  }
  return STATUS_ERROR;
}


static void
close_response(client_t *client)
{
  //send all response
  //closing reponse object
  client->response_closed = 1;
}


static VALUE
rb_body_iterator(VALUE iterator)
{
  return rb_funcall(iterator, i_next, 0);
}


static VALUE
ret_qnil(void)
{
  return Qnil;
}


static response_status
processs_write(client_t *client)
{
  VALUE iterator;
  VALUE item;
  char *buf;
  size_t buflen;
  write_bucket *bucket;
  int ret;

  // body
  DEBUG("process_write start");
  iterator = client->response_iter;

  while ( (item = rb_rescue(rb_body_iterator, iterator, ret_qnil, NULL)) != Qnil ) {

    buf = StringValuePtr(item);
    buflen = RSTRING_LEN(item);

    //write
    if(client->chunked_response){
      bucket = new_write_bucket(client->fd, 4);
      if(bucket == NULL){
	return STATUS_ERROR;
      }
      char *lendata = NULL;
      ssize_t len = 0;

      VALUE chunk_data = get_chunk_data(buflen);
      //TODO CHECK ERROR
      lendata = StringValuePtr(chunk_data);
      len = RSTRING_LEN(chunk_data);

      set_chunked_data(bucket, lendata, len, buf, buflen);
      bucket->chunk_data = chunk_data;
    } else {
      bucket = new_write_bucket(client->fd, 1);
      if(bucket == NULL){
	return STATUS_ERROR;
      }
      set2bucket(bucket, buf, buflen);
    }
    bucket->temp1 = item;

    ret = writev_bucket(bucket);
    if(ret != STATUS_OK){
      client->bucket = bucket;
      return ret;
    }

    free_write_bucket(bucket);
    //mark
    client->write_bytes += buflen;
    //check write_bytes/content_length
    if(client->content_length_set){
      if(client->content_length <= client->write_bytes){
	// all done
	break;
      }
    }

    if(client->chunked_response){
      DEBUG("write last chunk");
      //last packet
      bucket = new_write_bucket(client->fd, 3);
      if(bucket == NULL){
	return STATUS_ERROR;
      }
      set_last_chunked_data(bucket);
      writev_bucket(bucket);
      free_write_bucket(bucket);
    }
    close_response(client);
  }
  return STATUS_OK;
}


static response_status
process_body(client_t *client)
{
  response_status ret;
  write_bucket *bucket;
  if(client->bucket){
    bucket = (write_bucket *)client->bucket;
    //retry send
    ret = writev_bucket(bucket);
    
    if(ret == STATUS_OK){
      client->write_bytes += bucket->total_size;
      //free
      free_write_bucket(bucket);
      client->bucket = NULL;
    }else if(ret == STATUS_ERROR){
      free_write_bucket(bucket);
      client->bucket = NULL;
      return ret;
    }else{
      return ret;
    }
  }
  return processs_write(client);
}


static response_status
start_response_write(client_t *client)
{
  VALUE item;
  char *buf = NULL;
  ssize_t buflen = NULL;
    
  item = rb_rescue(rb_body_iterator, client->response_iter, ret_qnil, NULL);
  DEBUG("client %p :fd %d", client, client->fd);

  if (item != Qnil) {
    //write string only
    buf = StringValuePtr(item);
    buflen = RSTRING_LEN(item);
  }

  /* DEBUG("status_code %d body:%.*s", client->status_code, (int)buflen, buf); */
  return write_headers(client, buf, buflen);
}


response_status
response_start(client_t *client)
{
  response_status ret;

  if(client->status_code == 304){
    return write_headers(client, NULL, 0);
  }
  ret = start_response_write(client);
  DEBUG("start_response_write ret = %d :fd = %d\n", ret, client->fd);

  if(ret == STATUS_OK){
    // sended header
    ret = processs_write(client);
  }
  return ret;
}


static void
key_upper(char *s, const char *key, size_t len)
{
  int i = 0;
  int c;
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


static int
write_body2file(request *req, const char *buffer, size_t buf_len)
{
  FILE *tmp = (FILE *)req->body;
  fwrite(buffer, 1, buf_len, tmp);
  req->body_readed += buf_len;
  DEBUG("write_body2file %d bytes", (int)buf_len);
  return req->body_readed;
}


static int
write_body2mem(request *req, const char *buffer, size_t buf_len)
{
  rb_funcall((VALUE)req->body, i_write, 1, rb_str_new(buffer, buf_len));
  req->body_readed += buf_len;
  DEBUG("write_body2mem %d bytes \n", buf_len);
  return req->body_readed;
}


static int
write_body(request *req, const char *buffer, size_t buffer_len)
{
  if (req->body_type == BODY_TYPE_TMPFILE) {
    return write_body2file(req, buffer, buffer_len);
  } else {
    return write_body2mem(req, buffer, buffer_len);
  }
}


static client_t *
get_client(http_parser *p)
{
  return (client_t *)p->data;
}


static request *
get_current_request(http_parser *p)
{
  client_t *client = (client_t *)p->data;
  return client->current_req;
}


static VALUE
new_environ(client_t *client)
{
  VALUE object, environ;
  char r_port[7];

  environ = rb_hash_new();

  rb_hash_aset(environ, version_key, version_val);
  rb_hash_aset(environ, scheme_key, scheme_val);
  rb_hash_aset(environ, errors_key, errors_val);
  rb_hash_aset(environ, multithread_key, multithread_val);
  rb_hash_aset(environ, multiprocess_key, multiprocess_val);
  rb_hash_aset(environ, run_once_key, run_once_val);
  rb_hash_aset(environ, script_key, empty_string);
  rb_hash_aset(environ, server_name_key, server_name_val);
  rb_hash_aset(environ, server_port_key, server_port_val);

  object = rb_str_new2(client->remote_addr);
  rb_hash_aset(environ, rb_remote_addr, object);

  sprintf(r_port, "%d", client->remote_port);
  object = rb_str_new2(r_port);
  rb_hash_aset(environ, rb_remote_port, object);
  return environ;
}


static int
hex2int(int i)
{
  i = toupper(i);
  i = i - '0';
  if(i > 9){
    i = i - 'A' + '9' + 1;
  }
  return i;
}


static int
urldecode(char *buf, int len)
{
  int c, c1;
  char *s0, *t;
  t = s0 = buf;
  while(len >0){
    c = *buf++;
    if(c == '%' && len > 2){
      c = *buf++;
      c1 = c;
      c = *buf++;
      c = hex2int(c1) * 16 + hex2int(c);
      len -= 2;
    }
    *t++ = c;
    len--;
  }
  *t = 0;
  return t - s0;
}


static VALUE
concat_string(VALUE o, const char *buf, size_t len)
{
  VALUE ret;
  size_t l;
  char *dest, *origin;
    
  l = RSTRING_LEN(o);

  ret = rb_str_new((char*)0, l + len);
  if(ret == NULL){
    return ret;
  }
  dest = StringValuePtr(ret);
  origin = StringValuePtr(o);
  memcpy(dest, origin, l);
  memcpy(dest + l, buf, len);
  return ret;
}


static int
replace_env_key(VALUE dict, VALUE old_key, VALUE new_key)
{
  int ret = 1;
  VALUE value = rb_hash_aref(dict, old_key);

  if (value != Qnil) {
    rb_hash_delete(dict, old_key);
    rb_hash_aset(dict, new_key, value);
  }
  return ret;
}


static int
set_query(VALUE env, char *buf, int len)
{
  int c, slen = 0;
  char *s0;
  VALUE obj;

  s0 = buf;
  while(len > 0){
    c = *buf++;
    if(c == '#'){
      slen++;
      break;
    }
    len--;
    slen++;
  }

  if(slen > 1){
    obj = rb_str_new(s0, slen-1);
    rb_hash_aset(env, query_string, obj);
  }
  return 1; 
}


static int
set_path(VALUE env, char *buf, int len)
{
  int c, c1, slen;
  char *s0, *t;
  VALUE obj;

  t = s0 = buf;
  while(len > 0){
    c = *buf++;
    if(c == '%' && len > 2){
      c = *buf++;
      c1 = c;
      c = *buf++;
      c = hex2int(c1) * 16 + hex2int(c);
      len -= 2;
    }else if(c == '?'){
      //stop
      if(set_query(env, buf, len) == -1){
	//Error
	return -1;
      }
      break;
    }else if(c == '#'){
      //stop 
      //ignore fragment
      break;
    }
    *t++ = c;
    len--;
  }
  slen = t - s0;
  slen = urldecode(s0, slen);

  obj = rb_str_new(s0, slen);

  rb_hash_aset(env, path_info, obj);

  if (rb_hash_aref(env, query_string) == Qnil) {
    rb_hash_aset(env, query_string, empty_string);
  }
  return slen;
}


static VALUE
get_http_header_key(const char *s, int len)
{
  VALUE obj;
  char *dest;
  char c;

  obj = rb_str_new("", len + prefix_len);
  dest = (char*)StringValuePtr(obj);

  *dest++ = 'H';
  *dest++ = 'T';
  *dest++ = 'T';
  *dest++ = 'P';
  *dest++ = '_';

  while(len--) {
    c = *s++;
    if(c == '-'){
      *dest++ = '_';
    }else if(c >= 'a' && c <= 'z'){
      *dest++ = c - ('a'-'A');
    }else{
      *dest++ = c;
    }
  }
  return obj;
}


static int
message_begin_cb(http_parser *p)
{
  request *req;

  DEBUG("message_begin_cb");
    
  client_t *client = get_client(p);

  req = new_request();
  req->start_msec = current_msec;
  client->current_req = req;
  client->complete = 0;
  req->environ = new_environ(client);
  rb_gc_register_address(&req->environ);
  push_request(client->request_queue, client->current_req);
  return 0;
}


static int
header_field_cb(http_parser *p, const char *buf, size_t len)
{
  VALUE env, obj;
  request *req = get_current_request(p);
  
  if (req->last_header_element != FIELD){
    env = req->environ;
    if(LIMIT_REQUEST_FIELDS <= req->num_headers){
      req->bad_request_code = 400;
      return -1;
    }
    rb_hash_aset(env, req->field, req->value);
    req->field = NULL;
    req->value = NULL;
    req->num_headers++;
  }
  
  if(likely(req->field == NULL)){
    obj = get_http_header_key(buf, len);
  }else{
    char temp[len];
    key_upper(temp, buf, len);
    obj = concat_string(req->field, temp, len);
  }

  if(unlikely(obj == NULL)){
    req->bad_request_code = 500;
    return -1;
  }

  if(unlikely(RSTRING_LEN(obj) > LIMIT_REQUEST_FIELD_SIZE)){
    req->bad_request_code = 400;
    return -1;
  }

  req->field = obj;
  req->last_header_element = FIELD;
  return 0;
}


static int
header_value_cb(http_parser *p, const char *buf, size_t len)
{
  request *req = get_current_request(p);
  VALUE obj;
    
  if(likely(req->value == NULL)){
    obj = rb_str_new(buf, len);
  }else{
    obj = concat_string(req->value, buf, len);
  }
  
  if(unlikely(obj == NULL)){
    req->bad_request_code = 500;
    return -1; 
  }
  if(unlikely(RSTRING_LEN(obj) > LIMIT_REQUEST_FIELD_SIZE)){
    req->bad_request_code = 400;
    return -1;
  }
  req->value = obj;
  req->last_header_element = VAL;
  return 0;
}


static int
request_uri_cb(http_parser *p, const char *buf, size_t len)
{
  request *req = get_current_request(p);
  buffer_result ret = MEMORY_ERROR;
    
  if(req->path){
    ret = write2buf(req->path, buf, len);
  }else{
    req->path = new_buffer(1024, LIMIT_URI);
    ret = write2buf(req->path, buf, len);
  }
  switch(ret){
  case MEMORY_ERROR:
    req->bad_request_code = 500;
    return -1;
  case LIMIT_OVER:
    req->bad_request_code = 400;
    return -1;
  default:
    break;
  }
  return 0;
}


static int
body_cb(http_parser *p, const char *buf, size_t len)
{
  request *req = get_current_request(p);

  if(max_content_length <= req->body_readed + len){
    req->bad_request_code = 413;
    return -1;
  }
  if(req->body_type == BODY_TYPE_NONE){
    if(req->body_length == 0){
      //Length Required
      req->bad_request_code = 411;
      return -1;
    }
    if(req->body_length > client_body_buffer_size){
      //large size request
      FILE *tmp = tmpfile();
      if(tmp < 0){
	req->bad_request_code = 500;
	return -1;
      }
      req->body = tmp;
      req->body_type = BODY_TYPE_TMPFILE;
      DEBUG("BODY_TYPE_TMPFILE");
    }else{
      //default memory stream
      DEBUG("client->body_length %d \n", req->body_length);
      req->body = rb_funcall(StringIO, i_new, 1, rb_str_new2(""));
      req->body_type = BODY_TYPE_BUFFER;
      DEBUG("BODY_TYPE_BUFFER");
    }
  }
  write_body(req, buf, len);
  return 0;
}


int
headers_complete_cb(http_parser *p)
{
  VALUE obj = NULL;
  client_t *client = get_client(p);
  request *req = client->current_req;
  VALUE env = req->environ;
  int ret;
  uint64_t content_length = 0;

  DEBUG("should keep alive %d", http_should_keep_alive(p));
  client->keep_alive = http_should_keep_alive(p);

  if(p->content_length != ULLONG_MAX){
    content_length = p->content_length;
    if(max_content_length < p->content_length){
      RDEBUG("max_content_length over %d/%d", (int)content_length, (int)max_content_length);
      DEBUG("set request code %d", 413);
      req->bad_request_code = 413;
      return -1;
    }
  }

  if (p->http_minor == 1) {
    obj = http_11;
  } else {
    obj = http_10;
  }
  rb_hash_aset(env, server_protocol, obj);

  if(likely(req->path)){
    ret = set_path(env, req->path->buf, req->path->len);
    free_buffer(req->path);
    if(unlikely(ret == -1)){
      //TODO Error
      return -1;
    }
  }else{
    rb_hash_aset(env, path_info, default_path_string);
  }
  req->path = NULL;

  //Last header
  if(likely(req->field && req->value)){
    rb_hash_aset(env, req->field, req->value);

    req->field = NULL;
    req->value = NULL;
  }

  ret = replace_env_key(env, h_content_type, content_type);
  if(unlikely(ret == -1)){
    return -1;
  }
  ret = replace_env_key(env, h_content_length, content_length_key);
  if(unlikely(ret == -1)){
    return -1;
  }
     
  switch(p->method){
  case HTTP_DELETE:
    obj = http_delete;
    break;
  case HTTP_GET:
    obj = http_get;
    break;
  case HTTP_HEAD:
    obj = http_head;
    break;
  case HTTP_POST:
    obj = http_post;
    break;
  case HTTP_PUT:
    obj = http_put;
    break;
  case HTTP_CONNECT:
    obj = http_connect;
    break;
  case HTTP_OPTIONS:
    obj = http_options;
    break;
  case  HTTP_TRACE:
    obj = http_trace;
    break;
  case HTTP_COPY:
    obj = http_copy;
    break;
  case HTTP_LOCK:
    obj = http_lock;
    break;
  case HTTP_MKCOL:
    obj = http_mkcol;
    break;
  case HTTP_MOVE:
    obj = http_move;
    break;
  case HTTP_PROPFIND:
    obj = http_propfind;
    break;
  case HTTP_PROPPATCH:
    obj = http_proppatch;
    break;
  case HTTP_UNLOCK:
    obj = http_unlock;
    break;
  case HTTP_REPORT:
    obj = http_report;
    break;
  case HTTP_MKACTIVITY:
    obj = http_mkactivity;
    break;
  case HTTP_CHECKOUT:
    obj = http_checkout;
    break;
  case HTTP_MERGE:
    obj = http_merge;
    break;
  default:
    obj = http_get;
    break;
  }
    
  rb_hash_aset(env, request_method, obj);
  req->body_length = p->content_length;

  DEBUG("fin headers_complete_cb");
  return 0;
}


static int
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
  ,.on_url = request_uri_cb
  ,.on_body = body_cb
  ,.on_headers_complete = headers_complete_cb
  ,.on_message_complete = message_complete_cb
  };


static int
init_parser(client_t *cli, const char *name, const short port)
{
  cli->http_parser = (http_parser*)ruby_xmalloc(sizeof(http_parser));
  memset(cli->http_parser, 0, sizeof(http_parser));
  http_parser_init(cli->http_parser, HTTP_REQUEST);
  cli->http_parser->data = cli;

  return 0;
}


static size_t
execute_parse(client_t *cli, const char *data, size_t len)
{
  return http_parser_execute(cli->http_parser, &settings, data, len);
}


static int
parser_finish(client_t *cli)
{
  return cli->complete;
}


static void
setup_static_env(char *name, int port)
{
  char vport[7];

  prefix_len = strlen("HTTP_");

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

  content_type = rb_obj_freeze(rb_str_new2("CONTENT_TYPE"));
  content_length_key = rb_obj_freeze(rb_str_new2("CONTENT_LENGTH"));

  h_content_type = rb_obj_freeze(rb_str_new2("HTTP_CONTENT_TYPE"));
  h_content_length = rb_obj_freeze(rb_str_new2("HTTP_CONTENT_LENGTH"));

  http_10 = rb_obj_freeze(rb_str_new2("HTTP/1.0"));
  http_11 = rb_obj_freeze(rb_str_new2("HTTP/1.1"));

  http_delete = rb_obj_freeze(rb_str_new2("DELETE"));
  http_get = rb_obj_freeze(rb_str_new2("GET"));
  http_head = rb_obj_freeze(rb_str_new2("HEAD"));
  http_post = rb_obj_freeze(rb_str_new2("POST"));
  http_put = rb_obj_freeze(rb_str_new2("PUT"));
  http_connect = rb_obj_freeze(rb_str_new2("CONNECT"));
  http_options = rb_obj_freeze(rb_str_new2("OPTIONS"));
  http_trace = rb_obj_freeze(rb_str_new2("TRACE"));
  http_copy = rb_obj_freeze(rb_str_new2("COPY"));
  http_lock = rb_obj_freeze(rb_str_new2("LOCK"));
  http_mkcol = rb_obj_freeze(rb_str_new2("MKCOL"));
  http_move = rb_obj_freeze(rb_str_new2("MOVE"));
  http_propfind= rb_obj_freeze(rb_str_new2("PROPFIND"));
  http_proppatch = rb_obj_freeze(rb_str_new2("PROPPATCH"));
  http_unlock = rb_obj_freeze(rb_str_new2("UNLOCK"));
  http_report = rb_obj_freeze(rb_str_new2("REPORT"));
  http_mkactivity = rb_obj_freeze(rb_str_new2("MKACTIVITY"));
  http_checkout = rb_obj_freeze(rb_str_new2("CHECKOUT"));
  http_merge = rb_obj_freeze(rb_str_new2("MERGE"));

  http_user_agent = rb_obj_freeze(rb_str_new2("HTTP_USER_AGENT"));
  http_referer = rb_obj_freeze(rb_str_new2("HTTP_REFERER"));

  http_expect = rb_obj_freeze(rb_str_new2("HTTP_EXPECT"));
}


static int
setsig(int sig, void* handler)
{
  struct sigaction context, ocontext;
  context.sa_handler = handler;
  sigemptyset(&context.sa_mask);
  context.sa_flags = 0;
  return sigaction(sig, &context, &ocontext);
}


static void
setup_listen_sock(int fd)
{
  int on = 1, r = -1;
#ifdef linux
  r = setsockopt(fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &on, sizeof(on));
#endif
  r = fcntl(fd, F_SETFL, O_NONBLOCK);
  assert(r == 0);
}


static int
setup_sock(int fd)
{
  int r;
  int on = 1;
  r = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

  // 60 + 30 * 4
  /* on = 300; */
  /* r = setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &on, sizeof(on)); */
  /* assert(r == 0); */
  /* on = 30; */
  /* r = setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &on, sizeof(on)); */
  /* assert(r == 0); */
  /* on = 4; */
  /* r = setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &on, sizeof(on)); */
  /* assert(r == 0); */
#if linux
  r = 0; // Use accept4() on Linux
#else
  r = fcntl(fd, F_SETFL, O_NONBLOCK);
  assert(r == 0);
#endif
  return r;
}


static client_t *
new_client_t(int client_fd, char *remote_addr, uint32_t remote_port)
{
  client_t *client;
    
  client = ruby_xmalloc(sizeof(client_t));
  memset(client, 0, sizeof(client_t));
    
  client->fd = client_fd;
  client->remote_addr = remote_addr;
  client->remote_port = remote_port;
  client->request_queue = new_request_queue();
  return client;
}


static void
clean_client(client_t *client)
{
  VALUE environ;
  uintptr_t end, delta_msec = 0;

  request *req = client->current_req;

  if(log_fd) {
    write_access_log(client, log_fd, log_path);
    if(req){
      environ = req->environ;
      end = current_msec;
      if (req->start_msec > 0){
	delta_msec = end - req->start_msec;
    }
    } else {
      if (client->status_code != 408) {
	environ = new_environ(client);
      }
    }
  }

  if (req == NULL) {
    goto init;
  }

  DEBUG("status_code:%d env:%p", client->status_code, req->environ);
  if (req->body) {
    req->body = NULL;
  }
  free_request(req);
 init:
  client->current_req = NULL;
  client->header_done = 0;
  client->response_closed = 0;
  client->chunked_response = 0;
  client->content_length_set = 0;
  client->content_length = 0;
  client->write_bytes = 0;
}


static void
close_client(client_t *client)
{
  client_t *new_client = NULL;

  if (!client->response_closed) {
    close_response(client);
  }
  DEBUG("start close client:%p fd:%d status_code %d", client, client->fd, client->status_code);

  if (picoev_is_active(main_loop, client->fd)) {
    picoev_del(main_loop, client->fd);
    DEBUG("picoev_del client:%p fd:%d", client, client->fd);
  }

  clean_client(client);

  DEBUG("remain http pipeline size :%d", client->request_queue->size);
  if (client->request_queue->size > 0) {
    /* if (check_status_code(client) > 0) { */
    //process pipeline
    prepare_call_rack(client);
    call_rack_app(client);
    /* } */
    return;
  }

  if (client->http_parser != NULL) {
    ruby_xfree(client->http_parser);
  }

  free_request_queue(client->request_queue);
  if (!client->keep_alive) {
    close(client->fd);
    BDEBUG("close client:%p fd:%d", client, client->fd);
  } else {
    BDEBUG("keep alive client:%p fd:%d", client, client->fd);
    new_client = new_client_t(client->fd, client->remote_addr, client->remote_port);
    new_client->keep_alive = 1;
    init_parser(new_client, server_name, server_port);
    picoev_add(main_loop, new_client->fd, PICOEV_READ, keep_alive_timeout, r_callback, (void *)new_client);
  }
  //clear old client
  ruby_xfree(client);
}


static void
init_main_loop(void)
{
  if (main_loop == NULL) {
    /* init picoev */
    picoev_init(max_fd);
    /* create loop */
    main_loop = picoev_create_loop(60);
  }
}

static char*
get_reason_phrase(int status_code)
{
  if (status_code == 200) {
    return "OK";
  } else if (status_code == 201) {
    return "Created";
  } else if (status_code == 202) {
    return "Accepted";
  } else if (status_code == 203) {
    return "Non-Authoritative Information";
  } else if (status_code == 204) {
    return "No Content";
  } else if (status_code == 205) {
    return "Reset Content";
  } else if (status_code == 206) {
    return "Partial Content";
  } else if (status_code == 300) {
    return "Multiple Choices";
  } else if (status_code == 301) {
     return "Moved Permanently";
  } else if (status_code == 302) {
     return "Found";
  } else if (status_code == 303) {
     return "See Other";
  } else if (status_code == 304) {
     return "Not Modified";
  } else if (status_code == 305) {
     return "Use Proxy";
  } else if (status_code == 307) {
     return "Temporary Redirect";
  } else if (status_code == 400) {
     return "Bad Request";
  } else if (status_code == 401) {
     return "Unauthorized";
  } else if (status_code == 402) {
     return "Payment Required";
  } else if (status_code == 403) {
    return "Forbidden";
  } else if (status_code == 404) {
    return "Not Found";
  } else if (status_code == 405) {
    return "Method Not Allowed";
  } else if (status_code == 406) {
    return "Not Acceptable";
  } else if (status_code == 407) {
    return "Proxy Authentication Required";
  } else if (status_code == 408) {
    return "Request Time-out";
  } else if (status_code == 409) {
    return "Conflict";
  } else if (status_code == 410) {
    return "Gone";
  } else if (status_code == 411) {
    return "Length Required";
  } else if (status_code == 412) {
    return "Precondition Failed";
  } else if (status_code == 413) {
    return " Request Entity Too Large";
  } else if (status_code == 414) {
    return "Request-URI Too Large";
  } else if (status_code == 415) {
    return "Unsupported Media Type";
  } else if (status_code == 416) {
    return "Requested range not satisfiable";
  } else if (status_code == 417) {
    return "Expectation Failed";
  } else if (status_code == 500) {
    return "Internal Server Error";
  } else if (status_code == 501) {
    return "Not Implemented";
  } else if (status_code == 502) {
    return "Bad Gateway";
  } else if (status_code == 503) {
    return "Service Unavailable";
  } else if (status_code == 504) {
    return "Gateway Time-out";
  } else if (status_code == 505) {
    return "HTTP Version not supported";
  } else {
    return "Unknown";
  }
}


static int
process_rack_app(client_t *cli)
{
  VALUE env, response_arr, status_code, headers, response_body;
  request *req = cli->current_req;

  env = req->environ;

  // cli->response = [200, {}, []]
  response_arr = rb_funcall(rack_app, i_call, 1, env);

  // to_arr
  if (TYPE(response_arr) != T_ARRAY) {
    response_arr = rb_funcall(response_arr, i_toa, 0);
  }

  if(RARRAY_LEN(response_arr) < 3) {
    return 0;
  }
  
  status_code = rb_ary_entry(response_arr, 0);
  headers = rb_ary_entry(response_arr, 1);
  response_body = rb_ary_entry(response_arr, 2);

  if (TYPE(status_code) != T_FIXNUM ||
      TYPE(headers) != T_HASH       ||
      rb_respond_to(response_body, i_each) == 0) {
    return 0;
  }

  cli->status_code = NUM2INT(status_code);
  cli->headers = headers;
  cli->response_iter = rb_funcall(response_body, i_toenum, 0);

  rb_gc_register_address(&cli->headers);
  rb_gc_register_address(&cli->response_iter);

  if (cli->response_closed) {
    //closed
    close_client(cli);
    return 1;
  }

  errno = 0;
  /* printf("status code: %d\n", cli->status_code); */

  char* reason_phrase;
  reason_phrase = get_reason_phrase(cli->status_code);

  char buff[256];
  sprintf(buff, "HTTP/1.%d %d %s\r\n", cli->http_parser->http_minor, cli->status_code, reason_phrase);
  cli->http_status = rb_str_new(buff, strlen(buff));
  rb_gc_register_address(&cli->http_status);

  //check response
  if(cli->response && cli->response == Qnil){
    write_error_log(__FILE__, __LINE__);
    return 0;
  }
  return 1;
}


static void
w_callback(picoev_loop* loop, int fd, int events, void* cb_arg)
{
  client_t *client = ( client_t *)(cb_arg);
  int ret;
  DEBUG("call w_callback \n");
  if ((events & PICOEV_TIMEOUT) != 0) {
    YDEBUG("** w_callback timeout ** \n");
    //timeout
    client->keep_alive = 0;
    close_client(client);

  } else if ((events & PICOEV_WRITE) != 0) {
    ret = process_body(client);
    /* picoev_set_timeout(loop, client->fd, WRITE_TIMEOUT_SECS); */
    DEBUG("process_body ret %d \n", ret);
    if(ret != 0){
      //ok or die
      close_client(client);
    }
  }
}


static int
check_http_expect(client_t *client)
{
  VALUE c;
  char *val = NULL;
  int ret;
  request *req = client->current_req;

  if (client->http_parser->http_minor == 1) {
    ///TODO CHECK
    c = rb_hash_aref(req->environ, http_expect); 
    if (c != Qnil) {
      val = StringValuePtr(c);
      if (!strncasecmp(val, "100-continue", 12)) {
	ret = write(client->fd, "HTTP/1.1 100 Continue\r\n\r\n", 25);
	if (ret < 0) {
	  //fail
	  client->keep_alive = 0;
	  client->status_code = 500;
	  send_error_page(client);
	  close_client(client);
	  return -1;
	}
      } else {
	//417
	client->keep_alive = 0;
	client->status_code = 417;
	send_error_page(client);
	close_client(client);
	return -1;
      }
    }
    return 1;
  }
  return 0;
}


static void
call_rack_app(client_t *client)
{
  response_status ret;
  request *req = client->current_req;

  if(!process_rack_app(client)){
    //Internal Server Error
    req->bad_request_code = 500;
    send_error_page(client);
    close_client(client);
    return;
  }

  ret = response_start(client);
  switch(ret){
  case STATUS_ERROR:
    // Internal Server Error
    client->status_code = 500;
    send_error_page(client);
    close_client(client);
    return;
  case STATUS_SUSPEND:
    // continue
    // set callback
    DEBUG("set write callback %d \n", ret);
    //clear event
    picoev_del(main_loop, client->fd);
    picoev_add(main_loop, client->fd, PICOEV_WRITE, WRITE_TIMEOUT_SECS, w_callback, (void *)client);
    return;
  default:
    // send OK
    close_client(client);
  }
}


static inline void
set_current_request(client_t *client)
{
  request *req;
  req = shift_request(client->request_queue);
  client->current_req = req;
}


static void
prepare_call_rack(client_t *client)
{
  request *req = NULL;
  VALUE input, object;

  set_current_request(client);    
  req = client->current_req;

  //check Expect
  if (check_http_expect(client) < 0) {
    return;
  }

  if (req->body_type == BODY_TYPE_TMPFILE) {
    int fd;
    VALUE io;
    request *req = client->current_req;
    FILE *tmp = (FILE *)req->body;

    fflush(tmp);
    rewind(tmp);

    fd = fileno(tmp);
    io = rb_io_fdopen(fd, O_RDWR, NULL);
    rb_hash_aset(req->environ, rack_input, io);
  } else if(req->body_type == BODY_TYPE_BUFFER) {
    rb_funcall((VALUE)req->body, i_seek, 1, INT2NUM(0));
    rb_hash_aset(req->environ, rack_input, (VALUE)req->body);
  } else {
    object = rb_str_new2("");
    input = rb_funcall(StringIO, i_new, 1, object);
    rb_hash_aset(req->environ, rack_input, input);
    req->body = input;
  }

  if(!is_keep_alive){
    client->keep_alive = 0;
  }
}


static void
set_bad_request_code(client_t *client, int status_code)
{
  request *req;
  req = client->request_queue->tail;
  req->bad_request_code = status_code;
  DEBUG("set bad request code %d", status_code);
}


static int
check_status_code(client_t *client)
{
  request *req;
  req = client->request_queue->head;
  if (req && req->bad_request_code > 200) {
    //error
    //shift
    DEBUG("bad status code %d", req->bad_request_code);
    set_current_request(client);
    client->status_code = req->bad_request_code;
    send_error_page(client);
    close_client(client);
    return -1;
  }
  return 1;
}


static int
set_read_error(client_t *client, int status_code)
{
  client->keep_alive = 0;
  if (status_code == 0) {
    // bad request
    status_code = 400;
  }
  if (client->request_queue->size > 0) {
    //piplining
    set_bad_request_code(client, status_code);
    return 1;
  } else {
    client->status_code = status_code;
    send_error_page(client);
    close_client(client);
    return -1;
  }
}


static int
read_timeout(int fd, client_t *client)
{
  RDEBUG("** read timeout fd:%d", fd);
  //timeout
  return set_read_error(client, 408);
}


static int
parse_http_request(int fd, client_t *client, char *buf, ssize_t r)
{
  int nread = 0;
  request *req = NULL;

  BDEBUG("fd:%d \n%.*s", fd, (int)r, buf);
  nread = execute_parse(client, buf, r);
  BDEBUG("read request fd %d readed %d nread %d", fd, (int)r, nread);

  req = client->current_req;

  if (nread != r || req->bad_request_code > 0) {
    if (req == NULL) {
      DEBUG("fd %d bad_request code 400", fd);
      return set_read_error(client, 400);
    } else {
      DEBUG("fd %d bad_request code %d", fd,  req->bad_request_code);
      return set_read_error(client, req->bad_request_code);
    }
  }

  if (parser_finish(client) > 0) {
    return 1;
  }
  return 0;
}


static int
read_request(picoev_loop *loop, int fd, client_t *client, char call_time_update)
{
  char buf[READ_BUF_SIZE];
  ssize_t r;

  if (!client->keep_alive) {
    picoev_set_timeout(loop, fd, READ_TIMEOUT_SECS);
  }

  r = read(client->fd, buf, sizeof(buf));
  switch (r) {
  case 0: 
    return set_read_error(client, 503);
  case -1:
    // Error
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // try again later
      return 0;
    } else {
      // Fatal error
      client->keep_alive = 0;
      if (errno == ECONNRESET) {
	client->header_done = 1;
	client->response_closed = 1;
      }
      return set_read_error(client, 500);
    }
  default:
    if (call_time_update) {
      cache_time_update();
    }
    return parse_http_request(fd, client, buf, r);
  }
}


static void
r_callback(picoev_loop* loop, int fd, int events, void* cb_arg)
{
  client_t *cli = ( client_t *)(cb_arg);
  int finish = 0;

  if ((events & PICOEV_TIMEOUT) != 0) {
    finish = read_timeout(fd, cli);

  } else if ((events & PICOEV_READ) != 0) {
    finish = read_request(loop, fd, cli, 0);
  }
  if(finish == 1){
    picoev_del(loop, cli->fd);
    RDEBUG("del fd: %d", cli->fd);
    if (check_status_code(cli) > 0) {
      prepare_call_rack(cli);
      call_rack_app(cli);
    }
    return;
  }
}


static void
accept_callback(picoev_loop* loop, int fd, int events, void* cb_arg)
{
  int client_fd;
  client_t *client;
  struct sockaddr_in client_addr;
  char *remote_addr;
  uint32_t remote_port;
  int finish = 0;

  if ((events & PICOEV_TIMEOUT) != 0) {
    // time out
    // next turn or other process
    return;
  }else if ((events & PICOEV_READ) != 0) {
    int i;
    socklen_t client_len = sizeof(client_addr);
    for(i=0; i<8; ++i) {
#ifdef linux
      client_fd = accept4(fd, (struct sockaddr *)&client_addr, &client_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
      client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
#endif
      if (client_fd != -1) {
	DEBUG("accept fd %d \n", client_fd);

	if (setup_sock(client_fd) == -1) {
	  write_error_log(__FILE__, __LINE__);
	  // die
	  loop_done = 0;
	  return;
	}

	remote_addr = inet_ntoa(client_addr.sin_addr);
	remote_port = ntohs(client_addr.sin_port);
	client = new_client_t(client_fd, remote_addr, remote_port);
	init_parser(client, server_name, server_port);

	finish = read_request(loop, fd, client, 1);
	if (finish == 1) {
	  if (check_status_code(client) > 0) {
	    //current request ok
	    prepare_call_rack(client);
	    call_rack_app(client);
	  }
	} else if (finish == 0) {
	  picoev_add(loop, client_fd, PICOEV_READ, keep_alive_timeout, r_callback, (void *)client);
	}
      } else {
	if (errno != EAGAIN && errno != EWOULDBLOCK) {
	  write_error_log(__FILE__, __LINE__);
	  // die
	  loop_done = 0;
	}
      	break;
      }
    }
  }
}


static void
setup_server_env(void)
{
  setup_listen_sock(listen_sock);
  setup_sock(listen_sock);
  cache_time_init();
  setup_static_env(server_name, server_port);
}


static int
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
    return -1;
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((listen_sock = socket(p->ai_family, p->ai_socktype,
			      p->ai_protocol)) == -1) {
      continue;
    }
    
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &flag,
		   sizeof(int)) == -1) {
      close(listen_sock);
      return -1;
    }

    if (bind(listen_sock, p->ai_addr, p->ai_addrlen) == -1) {
      close(listen_sock);
      return -1;
    }
    break;
  }

  if (p == NULL) {
    close(listen_sock);
    rb_raise(rb_eIOError, "server: failed to bind\n");
  }

  freeaddrinfo(servinfo); // all done with this structure
    
  // BACKLOG 1024
  if (listen(listen_sock, backlog) == -1) {
    close(listen_sock);
    return -1;
  }
  setup_server_env();
  return 1;
}


static int
check_unix_sockpath(char *sock_name)
{
  if(!access(sock_name, F_OK)){
    if(unlink(sock_name) < 0){
      rb_raise(rb_eIOError, "failed to access sock_name\n");
    }
  }
  return 1;
}


static int
unix_listen(char *sock_name)
{
  int flag = 1;
  struct sockaddr_un saddr;
  mode_t old_umask;

  DEBUG("unix domain socket %s\n", sock_name);
  memset(&saddr, 0, sizeof(saddr));
  check_unix_sockpath(sock_name);

  if ((listen_sock = socket(AF_UNIX, SOCK_STREAM,0)) == -1) {
    rb_raise(rb_eIOError, "server: failed to listen sock\n");
  }

  if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &flag,
		 sizeof(int)) == -1) {
    close(listen_sock);
    rb_raise(rb_eIOError, "server: failed to set sockopt\n");
  }

  saddr.sun_family = PF_UNIX;
  strcpy(saddr.sun_path, sock_name);

  old_umask = umask(0);

  if (bind(listen_sock, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
    close(listen_sock);
    rb_raise(rb_eIOError, "server: failed to bind\n");
  }
  umask(old_umask);

  // BACKLOG 1024
  if (listen(listen_sock, backlog) == -1) {
    close(listen_sock);
    rb_raise(rb_eIOError, "server: failed to set backlog\n");
  }

  unix_sock_name = sock_name;
  return 1;
}


static void 
sigint_cb(int signum)
{
  loop_done = 0;
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

  if(!strcasecmp(log_path, "stdout")){
    log_fd = 1;
    return Qnil;
  }

  if(!strcasecmp(log_path, "stderr")){
    log_fd = 2;
    return Qnil;
  }

  log_fd = open_log_file(log_path);

  if(log_fd < 0){
    rb_raise(rb_eTypeError, "not open file. %s", log_path);
  }
  return Qnil;
}


static VALUE
bossan_listen(int argc, VALUE *argv, VALUE self)
{
  int ret;
  VALUE args1, args2;

  rb_scan_args(argc, argv, "11", &args1, &args2);

  if(listen_sock > 0){
    rb_raise(rb_eException, "already set listen socket");
  }

  if (argc == 2){
    server_name = StringValuePtr(args1);
    server_port = NUM2INT(args2);

    long _port = NUM2INT(args2);

    if (_port <= 0 || _port >= 65536) {
      // out of range
      rb_raise(rb_eArgError, "port number outside valid range");
    }

    server_port = (short)_port;

    ret = inet_listen();
  } else {
    Check_Type(args1, T_STRING);
    ret = unix_listen(StringValuePtr(args1));
  }

  if(ret < 0){
    //error
    listen_sock = -1;
  }

  if(listen_sock <= 0){
    rb_raise(rb_eTypeError, "not found listen socket");
  }

  return Qnil;
}

static VALUE
bossan_run_loop(VALUE self, VALUE args)
{
  rack_app = args;
    
  /* init picoev */
  init_main_loop();
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

  if(unix_sock_name){
    unlink(unix_sock_name);
  }
  printf("Bye.\n");
  return Qnil;
}


static VALUE 
bossan_set_max_content_length(VALUE self, VALUE args)
{
  max_content_length = NUM2INT(args);
  return Qnil;
}


static VALUE 
bossan_get_max_content_length(VALUE self)
{
  return INT2NUM(max_content_length);
}


static VALUE 
bossan_set_keepalive(VALUE self, VALUE args)
{
  int on;

  on = NUM2INT(args);
  if(on < 0){
    printf("keep alive value out of range.\n");
    return Qfalse;
  }

  is_keep_alive = on;

  if(is_keep_alive){
    keep_alive_timeout = on;
  }else{
    keep_alive_timeout = 2;
  }
  return Qnil;
}


static VALUE
bossan_get_keepalive(VALUE self)
{
  return INT2NUM(is_keep_alive);
}


static VALUE 
bossan_set_picoev_max_fd(VALUE self, VALUE args)
{
  int temp;
  temp = NUM2INT(args);
  if (temp <= 0) {
    rb_raise(rb_eException, "max_fd value out of range ");
  }
  max_fd = temp;
  return Qnil;
}


static VALUE 
bossan_get_picoev_max_fd(VALUE self)
{
  return INT2NUM(max_fd);
}


static VALUE
bossan_set_backlog(VALUE self, VALUE args)
{
  int temp;
  temp = NUM2INT(args);
  if (temp <= 0) {
    rb_raise(rb_eException, "backlog value out of range ");
  }
  backlog = temp;
  return Qnil;
}


static VALUE 
bossan_get_backlog(VALUE self)
{
  return INT2NUM(backlog);
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

  rb_gc_register_address(&h_content_type);
  rb_gc_register_address(&h_content_length);
  rb_gc_register_address(&content_type);
  rb_gc_register_address(&content_length_key);

  rb_gc_register_address(&http_10);
  rb_gc_register_address(&http_11);

  rb_gc_register_address(&http_delete);
  rb_gc_register_address(&http_get);
  rb_gc_register_address(&http_head);
  rb_gc_register_address(&http_post);
  rb_gc_register_address(&http_put);
  rb_gc_register_address(&http_connect);
  rb_gc_register_address(&http_options);
  rb_gc_register_address(&http_trace);
  rb_gc_register_address(&http_copy);
  rb_gc_register_address(&http_lock);
  rb_gc_register_address(&http_mkcol);
  rb_gc_register_address(&http_move);
  rb_gc_register_address(&http_propfind);
  rb_gc_register_address(&http_proppatch);
  rb_gc_register_address(&http_unlock);
  rb_gc_register_address(&http_report);
  rb_gc_register_address(&http_mkactivity);
  rb_gc_register_address(&http_checkout);
  rb_gc_register_address(&http_merge);

  rb_gc_register_address(&http_user_agent);
  rb_gc_register_address(&http_referer);

  rb_gc_register_address(&http_expect);

  empty_string = rb_obj_freeze(rb_str_new2(""));
  rb_gc_register_address(&empty_string);

  rb_gc_register_address(&i_keys);
  rb_gc_register_address(&i_call);
  rb_gc_register_address(&i_new);
  rb_gc_register_address(&i_each);
  rb_gc_register_address(&i_toenum);
  rb_gc_register_address(&i_close);
  rb_gc_register_address(&i_write);
  rb_gc_register_address(&i_seek);

  rb_gc_register_address(&rack_app); //rack app

  i_new = rb_intern("new");
  i_call = rb_intern("call");
  i_keys = rb_intern("keys");
  i_each = rb_intern("each");
  i_toenum = rb_intern("to_enum");
  i_close = rb_intern("close");
  i_write = rb_intern("write");
  i_seek = rb_intern("seek");
  i_toa = rb_intern("to_a");
  i_next = rb_intern("next");

  server = rb_define_module("Bossan");
  rb_gc_register_address(&server);

  rb_define_module_function(server, "listen", bossan_listen, -1);
  rb_define_module_function(server, "run", bossan_run_loop, 1);
  rb_define_module_function(server, "stop", bossan_stop, 0);
  rb_define_module_function(server, "shutdown", bossan_stop, 0);

  rb_define_module_function(server, "access_log", bossan_access_log, 1);
  rb_define_module_function(server, "set_max_content_length", bossan_set_max_content_length, 1);
  rb_define_module_function(server, "get_max_content_length", bossan_get_max_content_length, 0);

  rb_define_module_function(server, "set_keepalive", bossan_set_keepalive, 1);
  rb_define_module_function(server, "get_keepalive", bossan_get_keepalive, 0);

  rb_define_module_function(server, "set_picoev_max_fd", bossan_set_picoev_max_fd, 1);
  rb_define_module_function(server, "get_picoev_max_fd", bossan_get_picoev_max_fd, 0);

  rb_define_module_function(server, "set_backlog", bossan_set_backlog, 1);
  rb_define_module_function(server, "get_backlog", bossan_get_backlog, 0);

  rb_require("stringio");
  StringIO = rb_const_get(rb_cObject, rb_intern("StringIO"));
}
