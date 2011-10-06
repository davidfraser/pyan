#include "pq.h"

#include <stdlib.h>
#include <string.h>

PQ *pq_create(size_t item_size, int max_items)
{
	PQ *pq = malloc(sizeof(PQ));
	pq->slot_size = item_size + sizeof(int);
	pq->data = malloc(pq->slot_size * max_items);
	pq->item_size = item_size;
	pq->max_items = max_items;
	pq->num_items = 0;
	return pq;
}

void pq_destroy(PQ *pq)
{
	free(pq->data);
	free(pq);
}

/** Move the best child into the given slot, then sift_up on that child. */
static void sift_up(PQ *pq, int slot)
{
	int *x = (int *) (pq->data + (pq->slot_size * slot));
	int *c1 = (int *) (pq->data + (pq->slot_size * (2*slot+1)));
	int *c2 = (int *) (pq->data + (pq->slot_size * (2*slot+2)));
	if (2*slot+2 >= pq->num_items)
	{
		memmove(x, pq->data + (pq->slot_size * (pq->num_items-1)), pq->slot_size);
		return;
	}
	if (*c1 < *c2)
	{
		memmove(x, c1, pq->slot_size);
		sift_up(pq, 2*slot+1);
	}
	else
	{
		memmove(x, c2, pq->slot_size);
		sift_up(pq, 2*slot+2);
	}
}

static int sift_down(PQ *pq, int slot, int priority)
{
	int pslot = (slot-1)/2;

	int *x = (int *) (pq->data + (pq->slot_size * slot));
	int *p = (int *) (pq->data + (pq->slot_size * pslot));

	if (slot > 0 && priority < *p)
	{
		memmove(x, p, pq->slot_size);
		return sift_down(pq, pslot, priority);
	}
	else
	{
		return slot;
	}
}

int pq_push(PQ *pq, int priority, void *item)
{
	int slot = pq->num_items;
	int *x;

	if (slot >= pq->max_items)
	{
		pq->num_items--;
		slot = pq->max_items-1;
	}

	if (slot > 0)
		slot = sift_down(pq, slot, priority);

	x = (int *) (pq->data + (pq->slot_size * slot));
	*x = priority;
	memmove(&x[1], item, pq->item_size);

	pq->num_items++;

	return 1;
}

int pq_pop(PQ *pq, int *priority, void *dest)
{
	if (pq->num_items == 0)
		return 0;

	if (priority)
		*priority = *(int *)(pq->data);

	if (dest)
		memmove(dest, pq->data + sizeof(int), pq->item_size);

	if (pq->num_items > 1)
		sift_up(pq, 0);

	pq->num_items--;

	return 1;
}
