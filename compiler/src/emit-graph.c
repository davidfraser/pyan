#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "queue.h"


int emit_function(FUNCTION *func, EMIT_FUNCTIONS *functions, void *data)
{
    GRAPH *graph = func->graph;
    
    QUEUE *queue = create_queue();
    HASH *done = create_hash(10, key_type_direct);
    
    queue_push(queue, tree_get_child(graph, 0));
    
    NODE *last = NULL;
    while (!queue_is_empty(queue))
    {
        NODE *vertex = queue_pop(queue);
        
        if (find_in_hash(done, vertex, sizeof(void *)))
            continue;
        
do_next:
        add_to_hash(done, vertex, sizeof(void *), (void *) 1);
        int label = (int) get_from_hash(graph->labels, vertex, sizeof(void *));
        HASH *predecessor_hash = get_from_hash(graph->backward, vertex, sizeof(void *));
        HASH_ITERATOR iter;
        hash_iterator(predecessor_hash, &iter);
        if (predecessor_hash && (predecessor_hash->num > 1 || (predecessor_hash->num == 1 && last != iter.entry->key)))
        {
            functions->emit_label(label, data);
        }
        functions->emit_comment(vertex, data);
        
        HASH *successor_hash = get_from_hash(graph->forward, vertex, sizeof(void *));
        NODE *successor;
        int successor_label;
        if (successor_hash)
        {
            hash_iterator(successor_hash, &iter);
            successor = iter.entry->key;
            successor_label = (int) get_from_hash(graph->labels, successor, sizeof(void *));
        }
        else
            successor = NULL;
        
        if (tree_is_type(vertex, STMT_ENTER))
        {
            functions->emit_enter(vertex, data);
        }
        else if (tree_is_type(vertex, STMT_EXIT))
        {
            functions->emit_exit(vertex, data);
            last = vertex;
            continue;
        }
        else if (tree_is_type(vertex, STMT_ASSIGN))
            functions->emit_assign(vertex, data);
        else if (tree_is_type(vertex, STMT_RETURN))
            functions->emit_return(vertex, data);
        else if (tree_is_type(vertex, STMT_TEST))
        {
            hash_iterator_next(&iter);
            NODE *branch = iter.entry->key;
            EDGE_TYPE branch_type = (EDGE_TYPE) iter.entry->data;
            int branch_label = (int) get_from_hash(graph->labels, branch, sizeof(void *));
            
            functions->emit_test(vertex, branch_type, branch_label, data);
            if (!find_in_hash(done, branch, sizeof(void *)))
                queue_push(queue, branch);
            
            /* Force label on next vertex, in case we jumped to it in the test's branch.
               Fixes a bug where the label is omitted just because the test was before it,
                by neglecting to notice that the test reaches it by a jump. */
            continue;
        }
        last = vertex;
        if (find_in_hash(done, successor, sizeof(void *)))
        {
            functions->emit_jump(successor_label, data);
            continue;
        }
        vertex = successor;
        if (vertex)
            goto do_next;
    }
    
    functions->emit_end(data);
    
    destroy_queue(queue);
    destroy_hash(done);
    
    return 1;
}
