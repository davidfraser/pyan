/*
 * opt-tests.c - Optimise constant tests.
 *
 * Copyright (C) 2011, Edmund Horner.
 */

#include "compiler.h"


static NODE *get_successor(GRAPH *graph, NODE *vertex, EDGE_TYPE type)
{
    HASH *subhash = get_from_hash(graph->forward, vertex, sizeof(void *));    
    HASH_ITERATOR iter;
    for (hash_iterator(subhash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        NODE *succ = iter.entry->key;
        EDGE_TYPE succ_type = (EDGE_TYPE) iter.entry->data;
        if (succ_type & type)
            return succ;
    }
    
    return NULL;
}


/*
 * ALGORITHM
 *
 * Every test of a constant expression gets turned into a jump.
 */ 
int optimise_constant_tests(MODULE *module, FUNCTION *func)
{
    int changed = 0;
    int i;
    
    for (i = 0; i < tree_num_children(func->graph); i++)
    {
        NODE *vertex = tree_get_child(func->graph, i);
        if (!vertex)
            continue;
        
        if (!tree_is_type(vertex, STMT_TEST))
            continue;

        EXPRESSION *expr = tree_get_child(vertex, 0);
        if (!tree_is_type(expr, EXPR_INTEGER))
            continue;
        
        int x = CAST_TO_INTEGER(expr)->value;
        
        EDGE_TYPE target_type = x ? EDGE_YES : EDGE_NO;
        NODE *target_succ = get_successor(func->graph, vertex, target_type);
        
        /* Replace with a jump.  Test vertex remains will be clean up by dead code removal. */
        replace_backward(func->graph, vertex, target_succ, 0);
        changed = 1;
    }
    
    return changed;
};
