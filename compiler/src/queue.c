#include "queue.h"

#include <stdlib.h>

QUEUE *create_queue(void)
{
    QUEUE *queue = malloc(sizeof(QUEUE));
    queue->head = NULL;
    queue->tail = NULL;
    
    return queue;
}


void destroy_queue(QUEUE *queue)
{
    while (!queue_is_empty(queue))
        queue_pop(queue);
    
    free(queue);
}


int queue_is_empty(QUEUE *queue)
{
    return queue->head == NULL;
}


void queue_push(QUEUE *queue, void *data)
{
    QUEUE_ITEM *item = malloc(sizeof(QUEUE_ITEM));
    item->next = NULL;
    item->data = data;
    
    if (queue->tail)
        queue->tail->next = item;
    else
        queue->head = item;
    
    queue->tail = item;
}


void *queue_pop(QUEUE *queue)
{
    if (queue_is_empty(queue))
        return NULL;
    
    QUEUE_ITEM *head = queue->head;
    queue->head = head->next;
    if (queue->head == NULL)
        queue->tail = NULL;
    
    void *data = head->data;
    free(head);
    
    return data;
}
