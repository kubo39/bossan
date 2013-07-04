#include "bossan.h"
#include "request.h"

request *
new_request(void)
{
  request *req = (request *)ruby_xmalloc(sizeof(request));
  memset(req, 0, sizeof(request));
  return req;
}


void
free_request(request *req)
{
  ruby_xfree(req);
}


request_queue*
new_request_queue(void)
{
  request_queue *q = NULL;
  q = (request_queue *)ruby_xmalloc(sizeof(request_queue));
  memset(q, 0, sizeof(request_queue));
  GDEBUG("alloc req queue %p", q);
  return q;
}


void
free_request_queue(request_queue *q)
{
  request *req, *temp_req;
  req = q->head;
  while(req){
    temp_req = req;
    req = (request *)temp_req->next;
    free_request(temp_req);
  }
  GDEBUG("dealloc req queue %p", q);
  ruby_xfree(q);
}

void
push_request(request_queue *q, request *req)
{
  if(q->tail){
    q->tail->next = req;
  }else{
    q->head = req;
  }
  q->tail = req;
  q->size++;
}


request*
shift_request(request_queue *q)
{
  request *req, *temp_req;
  req = q->head;
  if(req == NULL){
    return NULL;
  }
  temp_req = req;
  req = req->next;
  q->head = req;
  q->size--;
  return temp_req;
}
