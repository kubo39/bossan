#include "buffer.h"

#define LIMIT_MAX 1024 * 1024 * 1024

buffer_t *
new_buffer(size_t buf_size, size_t limit)
{
  buffer_t *buf;
  buf = ruby_xmalloc(sizeof(buffer_t));
  memset(buf, 0, sizeof(buffer_t));
  buf->buf = ruby_xmalloc(sizeof(char) * buf_size);
  buf->buf_size = buf_size;
  if(limit){
    buf->limit = limit;
  }else{
    buf->limit = LIMIT_MAX;
  }
  return buf;
}


buffer_result
write2buf(buffer_t *buf, const char *c, size_t l)
{
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


void
free_buffer(buffer_t *buf)
{
  ruby_xfree(buf->buf);
  ruby_xfree(buf);
}


VALUE
getRbString(buffer_t *buf)
{
  VALUE o;
  o = rb_enc_str_new(buf->buf, buf->len, u8_encoding);
  free_buffer(buf);
  return o;
}


char *
getString(buffer_t *buf)
{
  buf->buf[buf->len] = '\0';
  return buf->buf;
}
