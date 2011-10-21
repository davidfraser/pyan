#include "compiler.h"

#include <stdio.h>
#include <string.h>


char *add_string(MODULE *module, char *str, size_t len)
{
    HASH_ENTRY *he = find_in_hash(module->strings, str, len);
    if (he)
        return he->data;
    
    str = strndup(str, len);
    add_to_hash(module->strings, str, len, str);
    return str;
}


static void print_integer_node(NODE *node)
{
    INTEGER *integer = CAST_TO_INTEGER(node);
    printf("value %d", integer->value);
}


static void print_variable_node(NODE *node)
{
    VARIABLE *var = CAST_TO_VARIABLE(node);
    printf("name '%s'", var->name);
}


static void print_declaration_node(NODE *node)
{
    DECLARATION *decl = CAST_TO_DECLARATION(node);
    printf("name '%s'", decl->name);
}


static int print_table_entry(HASH_ENTRY *he, void *data)
{
    if (strcmp("$parent", he->key))
    {
        printf("    %s:", (char *) he->key);
        tree_print(he->data, 1);
    }
    return 0;
}


static void print_block_node(NODE *node)
{
    BLOCK *block = CAST_TO_BLOCK(node);
    printf("\n");
    walk_hash(block->table, print_table_entry, NULL);
}


static void print_graph_node(NODE *node)
{
    int i;
    GRAPH *graph = CAST_TO_GRAPH(node);
    
    printf("\n");
    
    for (i = 0; i < tree_num_children(node); i++)
    {
        HASH_ENTRY *he;
        
        printf("%d ->", i);
        he = find_in_hash(graph->forward, tree_get_child(node, i), sizeof(void *));
        if (he)
        {
            HASH *subhash = he->data;
            HASH_ITERATOR iter;
            for (hash_iterator(subhash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
            {
                NODE *n = iter.entry->key;
                HASH_ENTRY *he2 = find_in_hash(graph->labels, n, sizeof(void *));
                if (he2)
                    printf(" %d", (int) he2->data);
            }
        }
        printf("\n");
        
        tree_print(tree_get_child(node, i), 2);
    }
}


void register_node_types(void)
{
    #define BASE_TYPE(n, s, p) tree_register_node_type(n, #n, sizeof(s), p);
    #define NODE_TYPE(n, s, p) tree_register_node_type(n, #n, sizeof(s), p);
    #include "types.h"
    #undef BASE_TYPE
    #undef NODE_TYPE

    tree_register_node_printer(EXPR_INTEGER, print_integer_node);
    tree_register_node_printer(EXPR_VARIABLE, print_variable_node);
    tree_register_node_printer(DEF_VARIABLE, print_declaration_node);
    tree_register_node_printer(DEF_FUNCTION, print_declaration_node);
    tree_register_node_printer(STMT_BLOCK, print_block_node);
    tree_register_node_printer(GRAPH_NODE, print_graph_node);
}


#define BASE_TYPE(n, s, p) s *CAST_TO_##s(void *ptr) { if (!tree_check_type(ptr, n)) error("Invalid cast of type %d to %d!", ((NODE *) ptr)->type, n); return (s *) ptr; }
#define NODE_TYPE(n, s, p)
#include "types.h"
#undef BASE_TYPE
#undef NODE_TYPE
