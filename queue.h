#include "segel.h"

typedef enum {false, true} bool;

typedef struct Node{
    struct timeval arrival_time;
    struct Node* next;
    int sockfd;
} Node;

typedef struct Queue{
    int size;
    struct Node* head;
    struct Node* tail;
} Queue;

Node* createNode(int socketFd, struct timeval arr_time);
void insert(Queue* queue, int socketFd, struct timeval arr_time);
int pop(Queue* queue);

bool isEmpty(Queue* queue);
int removeBySocketfd(Queue* queue, int sockfd);
int removeByIndex(Queue* queue, int index);
