#ifndef __REQUEST_H__

#include "segel.h"

typedef struct {
    int thread_id;
    int thread_req_counter;
    int thread_static_req_counter;
    int thread_dynamic_req_counter;
} Thread_stats;

typedef struct {
    struct timeval arrival_time;
    struct timeval dispatch_interval;
    Thread_stats thread_stats;
} Request_stats;



int requestHandle(int fd, Request_stats stats);

#endif
