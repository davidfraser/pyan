#ifndef PQ_H
#define PQ_H

#include <stdlib.h>

typedef struct PQ
{
	char *data;
	size_t slot_size;
	size_t item_size;
	int max_items;
	int num_items;
} PQ;

extern PQ *pq_create(size_t item_size, int max_items);
extern void pq_destroy(PQ *pq);
extern int pq_push(PQ *pq, int priority, void *item);
extern int pq_pop(PQ *pq, int *priority, void *dest);

#endif
