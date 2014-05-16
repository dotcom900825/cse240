#include "prefetcher.h"
#include <stdio.h>

Prefetcher::Prefetcher() {
  int i;

  _ready = false;
  _nextReq.addr = 0;
  _end_of_queue = 0;
  _prev_addr = 0;
  _next_delta = 0;
  _depth = 0;
  _items = (struct item *)malloc(sizeof(struct item) * MAX_CAPACITY);
  if (_items) {
    for (i = 0; i < MAX_CAPACITY; i++) {
      _items[i].delta = 0;
      _items[i].prev = -1;
    }
  } else {
    printf("ERROR: items initialization failed");
  }
}

Prefetcher::~Prefetcher() {
  free(_items);
}

bool Prefetcher::hasRequest(u_int32_t cycle){
  _depth++;
  if (_depth > 3)
    _ready = false;
  else
    _ready = true;
  // printf("Has request, _depth %d, ready %d\n", _depth, _ready);
  return _ready;
}

Request Prefetcher::getRequest(u_int32_t cycle){
  int next_delta;

  if (_depth == 1)
    next_delta = _next_delta;
  else if (_depth == 2)
    next_delta = _next_delta - 32;
  else
    next_delta = _next_delta + 32;

  _nextReq.addr = _prev_addr + next_delta;
  //printf("Next address %d\n", _nextReq.addr);
  return _nextReq;
}

void Prefetcher::completeRequest(u_int32_t cycle){
  return;
}


/* find the previous entry in the global delta table with the same delta */
int Prefetcher::find_prev(int delta){
  int cur_enq = _end_of_queue;
  int i = 0;

  while(i < MAX_CAPACITY - 1){
    if (cur_enq == 0)
      cur_enq = MAX_CAPACITY - 1;
    else
      cur_enq--;

    if (_items[cur_enq].delta == delta) {
      return cur_enq;
    }
    i++;
  }

  return -1;
}

/*
 * Search the global table, with the current delta, find the next possible
 * delta based on global history.
 * We will add the most possible next distance to current request address,
 * formatting the next request address.
 */
int Prefetcher::find_next_req(int delta){
  int cur_enq = _end_of_queue;
  int candidate;
  int i;
  bool found = false;
  struct candidate array[10];
  int num = 0;
  int flip = 0;
  int max = 0;
  int index = 0;
  int next_pos = 0;
  int temp_delta = 0;
  bool first_cycle = true;

  /* Initialize the candidates array with 10 entrys */
  for (i = 0; i < 10; i++) {
    array[i].delta = 0;
    array[i].count = 0; //set counter to 0
  }

  while(1) {
    if (!first_cycle) {
      candidate = cur_enq + 1;
      if (candidate == MAX_CAPACITY){
        candidate = 0;
      }

      /* Insert the addr of candidate entry to candidates array */
      temp_delta = _items[candidate].delta;
      if (temp_delta) {
        for (i = 0; i < num; i++) {
          if (array[i].delta == temp_delta) {
            array[i].count++;
            break;
          }
        }

        if (i == num && num < 10) { //add new candidates
          array[num].delta = temp_delta;
          array[num].count++;
          num++;
        }
      }
    } else {
      first_cycle = false;
    }

    next_pos = _items[cur_enq].prev;
    if (next_pos == -1)
      break;
    if (next_pos >= cur_enq && flip)
      break;
    if (next_pos >= cur_enq)
      flip = 1;
    if (_items[next_pos].delta != delta)
      break;

   // printf("delta %d, enqueue %d, next_pos %d, cur_enq %d\n", delta, _end_of_queue, next_pos, cur_enq);
    cur_enq = next_pos;
  }

  for (i = 0; i < num; i++) {
   // printf(" candidate %d, 0x%x, %d\n", i, array[i].delta, array[i].prev);
    if (array[i].count > max) {
      max = array[i].count;
      index = i;
    }
  }

  if (max && array[index].delta)
    return array[index].delta;
  else
    return 0;
}

void Prefetcher::cpuRequest(Request req){
  int delta;
  int next_delta;

  if(!_ready && !req.HitL1) {
    delta = req.addr - _prev_addr;

    //TODO use LRU replacement
    _items[_end_of_queue].delta = delta;
    _items[_end_of_queue].prev = find_prev(delta);
    _end_of_queue++;
    if (_end_of_queue == MAX_CAPACITY){
      _end_of_queue = 0;
    }
    next_delta = find_next_req(delta);

    /* align next_delta to 64 */
    if (next_delta >= 0 && next_delta < 64) {
     printf("find candidate delta %d for 0x%x\n", next_delta, req.addr);
      next_delta = 64;
    } else if (next_delta < 0 && next_delta > -64){
      next_delta = -64;
    }

    _next_delta = next_delta;
    _ready = true;
    _prev_addr = req.addr;
    _depth = 0;
  }
}
