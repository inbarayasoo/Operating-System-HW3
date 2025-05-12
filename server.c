#include "queue.h"
#include "request.h"
#include "segel.h"

//
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//
#define STATIC 1
#define DYNAMIC 2

const struct timeval DEFAULT = {.tv_sec = 0,
                                        .tv_usec = 0
};
Queue* working_queue = &(Queue){.size = 0,
                             .head = NULL,
                             .tail = NULL
};
Queue* waiting_queue = &(Queue){.size = 0,
                             .head = NULL, 
                             .tail = NULL
};
pthread_t* thread_array;
pthread_mutex_t main_lock;
pthread_cond_t main_cond, block_cond, flush_cond;

void* wrapper_request_handle(void* args);
Request_stats calculate_stats(int thread_index);
void initialize_mutex_cond();
pthread_t* createThreadArray(int num_threads);
void schedalg_request(int connfd, struct timeval arrival_time, int queue_size, char *schedalg);
void process_requests(int thread_index);

void getargs(int* port, int* num_threads, int* queue_size, char** schedalg, int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <port> <num_threads> <queue_size> <schedalg>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *num_threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    *schedalg = strdup(argv[4]);
}

int main(int argc, char* argv[]) {
    int listenfd, connfd, port, clientlen, num_threads, queue_size;
    char* schedalg;
    struct sockaddr_in clientaddr;
    getargs(&port, &num_threads, &queue_size, &schedalg, argc, argv);
    initialize_mutex_cond();
    thread_array = createThreadArray(num_threads);
    int index_threads = 0;
    while (index_threads < num_threads) {
        pthread_create(&thread_array[index_threads], NULL, wrapper_request_handle, NULL);
        index_threads++;
    }
    listenfd = Open_listenfd(port);
    struct timeval arrival_time = DEFAULT;
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA*)&clientaddr, (socklen_t*)&clientlen);
        gettimeofday(&arrival_time, NULL);
        schedalg_request(connfd, arrival_time,  queue_size,  schedalg);
    }
}

void* wrapper_request_handle(void* args) {
    int thread_index = 0;
    while(1){
        if (thread_array[thread_index] == pthread_self()){
            break;
        }
        thread_index++;
    }
    process_requests(thread_index);
}

void initialize_mutex_cond() {
    if (pthread_mutex_init(&main_lock, NULL) != 0) {
        fprintf(stderr, "\nmutex init has failed\n");
        exit(1);
    }

    if (pthread_cond_init(&main_cond, NULL) != 0) {
        fprintf(stderr, "\ncond init has failed\n");
        exit(1);
    }

    if (pthread_cond_init(&block_cond, NULL) != 0) {
        fprintf(stderr, "\nblock_cond init has failed\n");
        exit(1);
    }

    if (pthread_cond_init(&flush_cond, NULL) != 0) {
        fprintf(stderr, "\nflush_cond init has failed\n");
        exit(1);
    }

}

pthread_t* createThreadArray(int num_threads) {
    thread_array = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
    if (thread_array == NULL) {
        fprintf(stderr, "\nthread pool malloc failed\n");
        exit(1);
    }
    return thread_array;
}


Request_stats calculate_stats(int thread_index) {
    Thread_stats thread_stats = {
        .thread_id = thread_index,
        .thread_req_counter = 0,
        .thread_static_req_counter = 0,
        .thread_dynamic_req_counter = 0,
    };

    Request_stats stats = {
         .arrival_time = DEFAULT,
         .dispatch_interval = DEFAULT,
        .thread_stats = thread_stats,
    };
    return stats;
}

void schedalg_request(int connfd, struct timeval arrival_time, int queue_size, char *schedalg) {
    pthread_mutex_lock(&main_lock);
    if (waiting_queue->size + working_queue->size < queue_size) {
        insert(waiting_queue, connfd, arrival_time);
        pthread_cond_signal(&main_cond);
    } else {
        if (strcmp(schedalg, "block") == 0) {
            while (waiting_queue->size + working_queue->size >= queue_size) {
                pthread_cond_wait(&block_cond, &main_lock);
            }
            insert(waiting_queue, connfd, arrival_time);
            pthread_cond_signal(&main_cond);
        } else if (strcmp(schedalg, "dt") == 0) {
            close(connfd);
        } else if (strcmp(schedalg, "dh") == 0) {
            insert(waiting_queue, connfd, arrival_time);
            close(pop(waiting_queue));
            pthread_cond_signal(&main_cond);
        } else if (strcmp(schedalg, "bf") == 0) {
            while (waiting_queue->size + working_queue->size > 0) {
                pthread_cond_wait(&flush_cond, &main_lock);
            }
            close(connfd);
        } else if (strcmp(schedalg, "random") == 0) {
            if (isEmpty(waiting_queue)) {
                close(connfd);
            } else {
                int index = 0;
                int init_size = waiting_queue->size;
                while (index < ((init_size + 1) / 2)) {
                    close(removeByIndex(waiting_queue, rand() % (waiting_queue->size)));
                    index++;
                }
                insert(waiting_queue, connfd, arrival_time);
                pthread_cond_signal(&main_cond);
            }
        } else {
            fprintf(stderr, "\nunspecified policy\n");
            exit(1);
        }
    }
    pthread_mutex_unlock(&main_lock);
}

void process_requests(int thread_index) {
    struct timeval current_time = DEFAULT;
    Request_stats stats = calculate_stats(thread_index);
    while (1) {
        pthread_mutex_lock(&main_lock);  
        while (isEmpty(waiting_queue))
            pthread_cond_wait(&main_cond, &main_lock);
        gettimeofday(&current_time, NULL);
        stats.arrival_time = waiting_queue->head->arrival_time;
        timersub(&current_time, &stats.arrival_time, &stats.dispatch_interval);
        int connfd = pop(waiting_queue);
        insert(working_queue, connfd, DEFAULT);
        stats.thread_stats.thread_req_counter++;
        pthread_mutex_unlock(&main_lock);  
        int type = requestHandle(connfd, stats);
        pthread_mutex_lock(&main_lock);  
        if (type == DYNAMIC)
            stats.thread_stats.thread_dynamic_req_counter++;
        if (type == STATIC)
            stats.thread_stats.thread_static_req_counter++;
        removeBySocketfd(working_queue, connfd);
        if (isEmpty(working_queue))
            pthread_cond_signal(&flush_cond);
        pthread_cond_signal(&block_cond);
        Close(connfd);
        pthread_mutex_unlock(&main_lock);  
    }
}


