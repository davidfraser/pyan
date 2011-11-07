#ifndef PQ_H
#define PQ_H

/**
 * A priority queue data structure.  Each item in it has an associated
 * priority, and items can be popped from the PQ in order of the priority.
 * Items with smaller priority are popped first.
 */

#include <stdlib.h>

/**
 * PQ structure; should really be opaque.  The only field likely to be of
 * direct interest is num_items -- and really only for read-only use!
 */
typedef struct PQ
{
    char *data;
    size_t slot_size;
    size_t item_size;
    int max_items;
    int num_items;
} PQ;

/**
 * Create a PQ, for use with items of the given size.
 *
 * @param item_size Size of each item in the PQ.
 * @param max_items Maximum number of items it will need to hold.
 * @return Pointer to new PQ object.
 */
extern PQ *pq_create(size_t item_size, int max_items);

/**
 * Destroy a PQ; free its storage.
 *
 * @param Pointer to the PQ to be destroyed.
 */
extern void pq_destroy(PQ *pq);

/**
 * Push an item onto the PQ.  If the PQ is full, i.e. the number of items on
 * it is equal to its maximum size when created, an existing item will be
 * discarded to make space for it.
 *
 * @param pq PQ to push onto.
 * @param priority Priority of item to push.
 * @param item Pointer to item data; exactly item_size bytes of item data will be read.
 * @return Always 1.
 */
extern int pq_push(PQ *pq, int priority, void *item);

/**
 * Pop an item from the PQ.
 *
 * @param pq PQ to pop from.
 * @param priority Pointer to where popped item's priority will be stored.
 * @param dest Pointer to where to store the popped item; exactly item_size bytes will be stored.
 * @return 1 if an item was popped, or 0 if the PQ was empty.
 */
extern int pq_pop(PQ *pq, int *priority, void *dest);

#endif
