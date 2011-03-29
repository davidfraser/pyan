#ifndef TREE_H
#define TREE_H

#include "list.h"

#include <stdlib.h>

typedef int NODE_TYPE;

typedef struct NODE
{
    NODE_TYPE type;
    size_t size;
    struct NODE *owner;
    LIST *children;
} NODE;

extern NODE *tree_create_node(NODE_TYPE type);
extern void tree_destroy_node(NODE *node);
extern void tree_add_child(NODE *parent, NODE *child);
extern void tree_add_before(NODE *parent, NODE *child, NODE *before);
extern void tree_remove_child(NODE *parent, NODE *child);
extern const char *tree_get_name(NODE *tree);
extern void tree_print(NODE *tree, int indent);
extern NODE *tree_copy(NODE *tree);

typedef void NODE_PRINTER(NODE *node);

typedef struct NODE_TYPE_DATA
{
    NODE_TYPE type;
    char *name;
    size_t size;
    NODE_PRINTER *printer;
    NODE_TYPE parent;
} NODE_TYPE_DATA;

extern void tree_register_node_type(int type, char *name, size_t size, int parent);
extern void tree_register_node_printer(int type, NODE_PRINTER printer);

extern int tree_check_type(NODE *ptr, NODE_TYPE t);

#define tree_is_type(tree, t) ((tree) != NULL && ((NODE *) (tree))->type == (t))

#define tree_num_children(tree) (((NODE *) tree)->children->size)
#define tree_get_child(tree, num) (((NODE *) tree)->children->items[(num)])

#ifndef TREE_C
#define tree_add_child(p, n) tree_add_child((NODE *) p, (NODE *) n)
#define tree_copy(n) tree_copy((NODE *) n)
#endif

#endif
