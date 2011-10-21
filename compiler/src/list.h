#ifndef LIST_H
#define LIST_H

#define DEFAULT_LIST_SIZE 10

typedef int (* LIST_FOREACH_CALLBACK) (void *item, void *data);

typedef struct LIST
{
    int size;
    int max;
    void **items;
} LIST;

extern LIST *list_create(void);
extern void list_destroy(LIST *list);
extern void list_append(LIST *list, void *item);
extern void list_insert_before(LIST *list, void *item, void *before);
extern void list_remove(LIST *list, void *item);
extern int list_foreach(LIST *list, LIST_FOREACH_CALLBACK callback, void *data);

#endif
