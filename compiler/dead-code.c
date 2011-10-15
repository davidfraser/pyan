/*
 * dead-code.c - Removal of dead code.
 *
 * Copyright (C) 2011, Edmund Horner.
 */

#include "compiler.h"


/*
 * ALGORITHM
 *
 * Every vertex with no predecessor is removed (except STMT_ENTER).
 */ 
int remove_dead_code(MODULE *module, FUNCTION *func)
{
    int changed;
    int i;
    
    for (i = 0; i < tree_num_children(func->graph); i++)
    {
        NODE *vertex = tree_get_child(func->graph, i);
        if (!vertex)
            continue;
        
        if (tree_is_type(vertex, STMT_ENTER))
            continue;
    
        /* Check whether there are any predecessors. */
        HASH *subhash = get_from_hash(func->graph->backward, vertex, sizeof(void *));    
        if (subhash)
        {
            HASH_ITERATOR iter;
            hash_iterator(subhash, &iter);
            if (hash_iterator_valid(&iter))
                continue;
        }
        
        /* No predecessors, so remove edges to successors and remove. */
        replace_forward(func->graph, vertex, NULL, 0);
        remove_vertex(func->graph, vertex);
        changed = 1;        
    }
    
    return changed;
};
