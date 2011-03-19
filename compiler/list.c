#include "list.h"

#include <stdlib.h>
#include <string.h>


LIST *list_create(void)
{
    LIST *list = malloc(sizeof (LIST));
    list->size = 0;
    list->max = DEFAULT_LIST_SIZE;
    list->items = malloc(list->max * sizeof (void *));
    return list;
}


void list_destroy(LIST *list)
{
    free(list->items);
    free(list);
}


void list_append(LIST *list, void *item)
{
    if (list->size >= list->max)
    {
        list->max = list->size * 2;
        list->items = realloc(list->items, list->max * sizeof (void *));
    }
    
    list->items[list->size] = item;
    list->size++;
}


void list_insert_before(LIST *list, void *item, void *before)
{
    int i;
    
    if (list->size >= list->max)
    {
        list->max = list->size * 2;
        list->items = realloc(list->items, list->max * sizeof (void *));
    }
    
    for (i = 0; i < list->size; i++)
        if (list->items[i] == before)
        {
            memmove(&list->items[i+1], &list->items[i], (list->size - i) * sizeof(void *));
            list->items[i] = item;
            list->size++;
            return;
        }
}


void list_remove(LIST *list, void *item)
{
    int i;
    
    for (i = 0; i < list->size; i++)
        if (list->items[i] == item)
        {
            memmove(&list->items[i], &list->items[i+1], (list->size - i - 1) * sizeof(void *));
            list->size--;
            return;
        }
}


int list_foreach(LIST *list, LIST_FOREACH_CALLBACK callback, void *data)
{
    int i;
    
    for (i = 0; i < list->size; i++)
    {
        int result = callback(list->items[i], data);
        if (result != 0)
            return result;
    }
    
    return 0;
}
