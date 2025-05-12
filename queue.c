#include "queue.h"
#include <stdlib.h>

bool isEmpty(Queue* queue){
    if(queue->head == NULL){
        return true;
    }
    else{
        return false;
    }
}

int removeBySocketfd(Queue* queue, int sockfd){
    if (queue->head == NULL){
        return -1; 
    }
    Node* ptr = queue->head;
    Node* prev = NULL; 
    while(ptr != NULL){
        if(ptr->sockfd == sockfd){
            if(prev == NULL){ 
                int result = pop(queue);
                return result;
            } else {
                queue->size--;
                if(ptr == queue->tail){
                    queue->tail = prev;
                }
                prev->next = ptr->next;
                free(ptr);
                return sockfd;
            }
        }
        prev = ptr; 
        ptr = ptr->next;
    }
    return -1;
}

void insert(Queue* queue, int socketFd, struct timeval arr_time) {
    Node* new_node = createNode(socketFd, arr_time); 
    queue->size++; 
    if(!isEmpty(queue)){
        queue->tail->next = new_node; 
    }
    else{
        queue->head = new_node;
    }
    queue->tail = new_node; 
    
}

Node* createNode(int socketFd, struct timeval arr_time) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    new_node->arrival_time = arr_time;
    new_node->next = NULL;
    new_node->sockfd = socketFd;
    return new_node;
}

int pop(Queue* queue){
    if (queue->head == NULL){
        return -1; 
    }
    queue->size--; 
    int fd = queue->head->sockfd; 
    Node* nextNode = queue->head->next; 
    free(queue->head); 
    queue->head = nextNode; 
    if(queue->head == NULL){
        queue->tail = NULL; 
    }
    return fd; 
}

// FOR BONUS RANDOM POLICY
int removeByIndex(Queue* queue, int index){
    if (isEmpty(queue)){
        return -1;
    }
    if (index >= queue->size || index < 0){
        return -1;
    }
    Node* ptr = queue->head;
    int counter = 0;
    while(ptr){
        if(counter == index){
            int fdToRemove = ptr->sockfd;
            removeBySocketfd(queue, fdToRemove);
            return fdToRemove;
        }
        else{
            ptr = ptr->next;
            counter++;
        }
    }
    return -1;
}

