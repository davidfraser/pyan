#include "dfa.h"
#include "compiler.h"
#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


DFA *create_dfa(FUNCTION *func, DFA_FUNCTIONS *functions, void *data, DFA_FLAGS flags)
{
    if ((flags & DFA_FORWARD && flags & DFA_BACKWARD) || (!(flags & DFA_FORWARD) && !(flags & DFA_BACKWARD)))
        error("create_dfa caller must specify one of DFA_FORWARD, DFA_BACKWARD.");
    DFA *dfa = malloc(sizeof(DFA));
    dfa->function = func;
    dfa->graph = func->graph;
    dfa->functions = functions;
    dfa->data = data;
    dfa->flags = flags;
    
    dfa->inputs = create_hash(10, key_type_direct);
    dfa->outputs = create_hash(10, key_type_direct);
    
    return dfa;
}


static void add_joins(DFA *dfa, GRAPH *graph)
{
    int i;
restart:
    for (i = 1; i < tree_num_children(graph); i++)
    {
        NODE *vertex = tree_get_child(graph, i);
        if (!vertex || tree_is_type(vertex, STMT_JOIN))
            continue;
        
        HASH *upstream = (dfa->flags & DFA_FORWARD) ? graph->backward : graph->forward;
        HASH *predecessor_hash = get_from_hash(upstream, vertex, sizeof(void *));
        if (predecessor_hash && predecessor_hash->num > 1)
        {
            int j;
            HASH_ITERATOR iter;
            NODE *join = CAST_TO_NODE(make_join(CAST_TO_AST(vertex)->source_line));
            add_vertex(graph, join);
            for (j = 0, hash_iterator(predecessor_hash, &iter); j < 2 && hash_iterator_valid(&iter); j++, hash_iterator_next(&iter))
            {
                NODE *predecessor = iter.entry->key;
                if (dfa->flags & DFA_FORWARD)
                {
                    add_edge(graph, predecessor, join, (EDGE_TYPE) iter.entry->data);
                    remove_edge(graph, predecessor, vertex);
                }
                else
                {
                    add_edge(graph, join, predecessor, (EDGE_TYPE) iter.entry->data);
                    remove_edge(graph, vertex, predecessor);
                }
            }
            if (dfa->flags & DFA_FORWARD)
                add_edge(graph, join, vertex, 1);
            else
                add_edge(graph, vertex, join, 1);
            goto restart;
        }
    }
}


static void create_sets(DFA *dfa)
{
    int i;
    HASH_ITERATOR iter;
    
    for (i = 0; i < tree_num_children(dfa->graph); i++)
    {
        NODE *vertex = tree_get_child(dfa->graph, i);
        if (!vertex)
            continue;
        
        LIST *input = list_create();
        LIST *output = list_create();
        
        add_to_hash(dfa->inputs, vertex, sizeof(void *), input);
        add_to_hash(dfa->outputs, vertex, sizeof(void *), output);
    }
    
    for (hash_iterator(dfa->graph->forward, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        HASH_ITERATOR iter2;
        HASH *subhash = iter.entry->data;
        for (hash_iterator(subhash, &iter2); hash_iterator_valid(&iter2); hash_iterator_next(&iter2))
        {
            NODE *from = iter.entry->key;
            NODE *to = iter2.entry->key;
            
            void *set = /*tree_is_type(from, STMT_ENTER)
                    ?*/ dfa->functions->create_start_set(dfa->data, (EDGE_TYPE) iter2.entry->data) /*
                    : dfa->functions->create_default_set(dfa->data, (EDGE_TYPE) iter2.entry->data)*/ ;
            
            LIST *input = get_from_hash(dfa->inputs, (dfa->flags & DFA_FORWARD) ? to : from, sizeof(void *));
            LIST *output = get_from_hash(dfa->outputs, (dfa->flags & DFA_FORWARD) ? from : to, sizeof(void *));
            
            list_append(input, set);
            list_append(output, set);
        }
    }
}


static void destroy_sets(DFA *dfa)
{
    HASH_ITERATOR iter;
    
    for (hash_iterator(dfa->inputs, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        int i;
        LIST *input = iter.entry->data;
        for (i = 0; i < input->size; i++)
            dfa->functions->destroy_set(input->items[i]);
        list_destroy(input);
    }
    
    for (hash_iterator(dfa->outputs, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        list_destroy(iter.entry->data);
    }
}


void destroy_dfa(DFA *dfa)
{
    destroy_sets(dfa);
    destroy_hash(dfa->inputs);
    destroy_hash(dfa->outputs);
    
    free(dfa);
}


int run_dfa(DFA *dfa)
{
    int result;
    
    /* Initialise. */
    if (dfa->flags & DFA_ADD_JOINS)
        add_joins(dfa, dfa->graph);
    
    create_sets(dfa);
    
    QUEUE *queue = create_queue();
    HASH *queued_items = create_hash(10, key_type_direct);
    
    /* Run DFA. */
    NODE *start = tree_get_child(dfa->graph, (dfa->flags & DFA_FORWARD) ? 0 : 1);
    queue_push(queue, start);
    add_to_hash(queued_items, start, sizeof(void *), (void *) 1);

    while (!queue_is_empty(queue))
    {
        NODE *vertex = queue_pop(queue);
        add_to_hash(queued_items, vertex, sizeof(void *), 0);
        
        LIST *input = get_from_hash(dfa->inputs, vertex, sizeof(void *));
        LIST *output = get_from_hash(dfa->outputs, vertex, sizeof(void *));
        
        int changed = dfa->functions->analyse(vertex, input, output, dfa->data);
        
        if (changed)
        {
            HASH *downstream = (dfa->flags & DFA_FORWARD) ? dfa->graph->forward : dfa->graph->backward;
            HASH *successor_hash = get_from_hash(downstream, vertex, sizeof(void *));
            HASH_ITERATOR iter;
            for (hash_iterator(successor_hash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
            {
                NODE *successor = iter.entry->key;
                if (!(int) get_from_hash(queued_items, successor, sizeof(void *)))
                {
                    queue_push(queue, successor);
                    add_to_hash(queued_items, successor, sizeof(void *), 0);
                }
            }
        }
    }
    
    //print_graph(dfa->graph, dfa->function->name, dfa);
    
    /* Verify. */
    int i;
    result = 1;
    for (i = 0; i < tree_num_children(dfa->graph); i++)
    {
        NODE *vertex = tree_get_child(dfa->graph, i);
        
        void *input = get_from_hash(dfa->inputs, vertex, sizeof(void *));
        void *output = get_from_hash(dfa->outputs, vertex, sizeof(void *));
        
        result &= dfa->functions->verify(vertex, input, output, dfa->data);
    }

    /* Clean up. */
    destroy_queue(queue);
    
    return result;
}
