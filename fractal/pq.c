#include "pq.h"

#include <stdlib.h>
#include <string.h>

PQ *pq_create(size_t item_size, int max_items)
{
    PQ *pq = malloc(sizeof(PQ));
    if (!pq)
        return NULL;
    pq->slot_size = item_size + sizeof(int);
    pq->data = malloc(pq->slot_size * max_items);
    if (!pq->data)
    {
        free(pq);
        return NULL;
    }
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
    int *c1, *c2;
    
    if (2*slot+2 >= pq->num_items)
    {
        memmove(x, pq->data + (pq->slot_size * (pq->num_items-1)), pq->slot_size);
        return;
    }
    
    c1 = (int *) (pq->data + (pq->slot_size * (2*slot+1)));
    c2 = (int *) (pq->data + (pq->slot_size * (2*slot+2)));
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

static void sift_up_4(PQ *pq, int slot)
{
    int *x = (int *) (pq->data + (4 * slot));
    int *c1, *c2;
    
    if (2*slot+2 >= pq->num_items)
    {
        int *last_item = (int *) (pq->data + (4 * (pq->num_items-1)));
        *x = *last_item;
        return;
    }
    
    c1 = (int *) (pq->data + (4 * (2*slot+1)));
    c2 = (int *) (pq->data + (4 * (2*slot+2)));
    if (*c1 < *c2)
    {
        *x = *c1;
        sift_up_4(pq, 2*slot+1);
    }
    else
    {
        *x = *c2;
        sift_up_4(pq, 2*slot+2);
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

static int sift_down_4(PQ *pq, int slot, int priority)
{
    int pslot = (slot-1)/2;

    int *x = (int *) (pq->data + (pq->slot_size * slot));
    int *p = (int *) (pq->data + (pq->slot_size * pslot));

    if (slot > 0 && priority < *p)
    {
        *x = *p;
        return sift_down_4(pq, pslot, priority);
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
    {
        if (pq->item_size == 0)
            slot = sift_down(pq, slot, priority);
        else
            slot = sift_down_4(pq, slot, priority);
    }

    x = (int *) (pq->data + (pq->slot_size * slot));
    *x = priority;
    if (item != NULL && pq->item_size != 0)
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

    if (dest && pq->item_size != 0)
        memmove(dest, pq->data + sizeof(int), pq->item_size);

    if (pq->num_items > 1)
    {
        if (pq->item_size == 0)
            sift_up_4(pq, 0);
        else
            sift_up(pq, 0);
    }

    pq->num_items--;

    return 1;
}
