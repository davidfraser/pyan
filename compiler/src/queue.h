#ifndef QUEUE_H
#define QUEUE_H

typedef struct QUEUE_ITEM
{
    struct QUEUE_ITEM *next;
    void *data;
} QUEUE_ITEM;

typedef struct QUEUE
{
    QUEUE_ITEM *head;
    QUEUE_ITEM *tail;
} QUEUE;

extern QUEUE *create_queue(void);
extern void destroy_queue(QUEUE *queue);
extern int queue_is_empty(QUEUE *queue);
extern void queue_push(QUEUE *queue, void *data);
extern void *queue_pop(QUEUE *queue);

#endif
