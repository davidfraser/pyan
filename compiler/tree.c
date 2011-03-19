#define TREE_C
#include "tree.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gc.h>

#include "hash.h"


static HASH *type_registry = NULL;


NODE *tree_create_node(NODE_TYPE type)
{
    NODE_TYPE_DATA *data = get_from_hash(type_registry, (void *) type, sizeof(void *));
    
    NODE *node = GC_MALLOC(data->size);
    
    node->type = type;
    node->size = data->size;
    node->owner = NULL;
    node->children = list_create();
    
    memset(((void *) node) + sizeof(NODE), 0, node->size - sizeof(NODE));

    return node;
}


static int tree_destroy_child(void *item, void *data)
{
    NODE *child = item;
    NODE *parent = data;
    
    if (child != NULL && child->owner == parent)
        tree_destroy_node(child);
    
    return 0;
}


void tree_destroy_node(NODE *node)
{
    list_foreach(node->children, tree_destroy_child, node);
    
    list_destroy(node->children);
    GC_FREE(node);
}


void tree_add_child(NODE *parent, NODE *child)
{
    list_append(parent->children, child);
}


void tree_add_before(NODE *parent, NODE *child, NODE *before)
{
    list_insert_before(parent->children, child, before);
}


void tree_remove_child(NODE *parent, NODE *child)
{
    list_remove(parent->children, child);
}


void tree_print(NODE *tree, int indent)
{
    int i;
    
    for (i = 0; i < indent; i++)
        printf("  ");
    
    if (tree == NULL)
    {
        printf("NULL\n");
        return;
    }
    
    NODE_TYPE_DATA *data = get_from_hash(type_registry, (void *) tree->type, sizeof(void *));
    
    printf("TREE type %s (%d)", data->name, tree->type);
    if (data->printer)
    {
        printf(" ");
        data->printer(tree);
    }
    printf("\n");
    for (i = 0; i < tree->children->size; i++)
    {
        tree_print(tree->children->items[i], indent+1);
    }
}


NODE *tree_copy(NODE *tree)
{
    int i;
    
    if (tree == NULL)
        return tree;
    
    NODE *new_tree = tree_create_node(tree->type);
    
    for (i = 0; i < tree->children->size; i++)
    {
        tree_add_child(new_tree, tree_copy(tree_get_child(tree, i)));
    }
    
    memmove(((void *) new_tree) + sizeof(NODE), ((void *) tree) + sizeof(NODE), tree->size - sizeof(NODE));
    
    return new_tree;
}


void tree_register_node_type(int type, char *name, size_t size, int parent)
{
    if (type_registry == NULL)
        type_registry = create_hash(10, key_type_direct);
    
    NODE_TYPE_DATA *data = get_from_hash(type_registry, (void *) type, sizeof(void *));
    if (!data)
    {
        data = GC_MALLOC(sizeof(NODE_TYPE_DATA));
        add_to_hash(type_registry, (void *) type, sizeof(void *), data);
    }
    data->type = type;
    data->name = name;
    data->size = size;
    data->printer = NULL;
    data->parent = parent;
}


void tree_register_node_printer(int type, NODE_PRINTER printer)
{
    NODE_TYPE_DATA *data = get_from_hash(type_registry, (void *) type, sizeof(void *));
    data->printer = printer;
}


int tree_check_type(NODE *ptr, NODE_TYPE t)
{
    NODE_TYPE type = ptr->type;
    while (type != 0)
    {
        if (type == t)
            return 1;
        NODE_TYPE_DATA *data = get_from_hash(type_registry, (void *) type, sizeof(void *));
        type = data->parent;
    }
    return 0;
}
