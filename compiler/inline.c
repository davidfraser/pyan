#include "compiler.h"

#include <string.h>
#include <stdio.h>


/**
 * Add the CFG for a function into another function's CFG.
 *
 * @return The label of the incoming graph's first (entry) vertex.
 */
static int add_cfg(MODULE *module, FUNCTION *dest, FUNCTION *src)
{
    GRAPH *dest_graph = dest->graph;
    GRAPH *src_graph = src->graph;
    int i;
    int base;
    
    base = tree_num_children(dest_graph);
    
    /* Copy vertices. */
    for (i = 0; i < tree_num_children(src_graph); i++)
    {
        NODE *src_vertex = tree_get_child(src_graph, i);
        NODE *dest_vertex = tree_copy(src_vertex);
        
        add_vertex(dest_graph, dest_vertex);
    }
    
    /* Copy edges. */
    for (i = 0; i < tree_num_children(src_graph); i++)
    {
        NODE *src_vertex = tree_get_child(src_graph, i);
        NODE *dest_vertex = tree_get_child(dest_graph, base+i);
        
        HASH *subhash = get_from_hash(src_graph->forward, src_vertex, sizeof(void *));
        HASH_ITERATOR iter;
        for (hash_iterator(subhash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
        {
            int label = (int) get_from_hash(src_graph->labels, iter.entry->key, sizeof(void *));
            NODE *dest_succ = tree_get_child(dest_graph, base+label);
            EDGE_TYPE type = (EDGE_TYPE) iter.entry->data;
            add_edge(dest_graph, dest_vertex, dest_succ, type);
        }
    }
    
    /* Copy symbol table. */
    HASH_ITERATOR iter;
    HASH *src_table = src->table;
    HASH *dest_table = dest->table;
    for (hash_iterator(src_table, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        if (!strcmp(iter.entry->key, "$parent"))
            continue;
        
        char next_name[100];
        sprintf(next_name, "$n%d%s", base, (char *) iter.entry->key);
    
        char *str = add_string(module, next_name, strlen(next_name));
        
        DECLARATION *decl = CAST_TO_DECLARATION(tree_copy(iter.entry->data));
        decl->flags &= ~DECL_ARGUMENT;
        
        add_to_hash(dest_table, str, strlen(str), decl);
    }
    
    return base;
}


#define INLINE_THRESHOLD 500


static int node_contains_call(NODE *node)
{
    if (!node)
        return 0;
    
    if (tree_is_type(node, EXPR_CALL))
        return 1;
    
    int i;
    for (i = 0; i < tree_num_children(node); i++)
    {
        if (node_contains_call(tree_get_child(node, i)))
            return 1;
    }
    
    return 0;
}


/**
 * Can this function be inlined into another (and is it potentially worth it) ?  A function is inlinable if:
 *   - it has a CFG.
 *   - its CFG does not contain calls.
 *   - it is smaller than the threshold.
 */
int is_inlinable(FUNCTION *func)
{
    if (!func)
        return 0;
    
    GRAPH *graph = func->graph;
    
    if (!graph)
        return 0;
    
    if (tree_num_children(graph) > INLINE_THRESHOLD)
        return 0;
    
    int i;
    for (i = 0; i < tree_num_children(graph); i++)
    {
        NODE *vertex = tree_get_child(graph, i);
        if (node_contains_call(vertex))
            return 0;
    }
    
    return 1;
}


static void rename_variables(MODULE *module, NODE *node, int base)
{
    if (!node)
        return;
    
    if (tree_is_type(node, EXPR_VARIABLE))
    {
        VARIABLE *var = CAST_TO_VARIABLE(node);
        char next_name[100];
        sprintf(next_name, "$n%d%s", base, var->name);
    
        char *str = add_string(module, next_name, strlen(next_name));
        
        var->name = str;
    }
    else
    {
        int i;
        for (i = 0; i < tree_num_children(node); i++)
            rename_variables(module, tree_get_child(node, i), base);
    }
}


static int analyse_vertex(MODULE *module, FUNCTION *func, NODE *vertex)
{
    /* Check that this vertex is a call to an inlinable function. */
    if (!tree_is_type(vertex, STMT_ASSIGN))
        return 0;
    
    EXPRESSION *expr = tree_get_child(vertex, 1);
    if (!tree_is_type(expr, EXPR_CALL))
        return 0;
    
    VARIABLE *fvar = tree_get_child(expr, 0);
    FUNCTION *called_func = get_from_hash(module->table, fvar->name, strlen(fvar->name));
    if (!is_inlinable(called_func))
    {
        fprintf(stderr, "Call to '%s' in '%s' is not inlinable\n", fvar->name, CAST_TO_DECLARATION(func)->name);
        return 0;
    }
    else
        fprintf(stderr, "Call to '%s' in '%s' is inlinable\n", fvar->name, CAST_TO_DECLARATION(func)->name);        
    
    /* Perform the inlining! */
    int base = add_cfg(module, func, called_func);
    GRAPH *graph = func->graph;
    NODE *entry = tree_get_child(graph, base);
    NODE *exit = tree_get_child(graph, base+1);
    
    EXPRESSION *in_tuple = get_input_tuple(called_func);
    rename_variables(module, CAST_TO_NODE(in_tuple), base);
    int i;
    for (i = base; i < tree_num_children(graph); i++)
        rename_variables(module, tree_get_child(graph, i), base);
    
    /* The entry is replaced with an assignment of the call's parameters into the called function's arguments. */
    EXPRESSION *args = tree_get_child(expr, 1);
    STATEMENT *new_assign = make_assignment(in_tuple, args);
    
    add_vertex(graph, CAST_TO_NODE(new_assign));
    replace_forward(graph, CAST_TO_NODE(entry), CAST_TO_NODE(new_assign), 0);
    remove_vertex(graph, CAST_TO_NODE(entry));
    
    /* Predecessors are repointed to the new assignment. */
    replace_backward(graph, CAST_TO_NODE(vertex), CAST_TO_NODE(new_assign), 0);
    
    /* Successor is repointed from the exit's predecessors. */
    HASH *subhash = get_from_hash(graph->forward, vertex, sizeof(void *));
    HASH_ITERATOR iter;
    hash_iterator(subhash, &iter);
    NODE *succ = iter.entry->key;
    remove_edge(graph, vertex, succ);
    
    replace_backward(graph, exit, succ, 0);
    
    /* Turn returns into assignments. */
    subhash = get_from_hash(graph->backward, succ, sizeof(void *));
    for (hash_iterator(subhash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        NODE *ret = iter.entry->key;
        
        if (tree_is_type(ret, STMT_RETURN))
        {
            STATEMENT *new_assign = make_assignment(tree_get_child(vertex, 0), tree_get_child(ret, 0));
            add_vertex(graph, CAST_TO_NODE(new_assign));
            
            replace_forward(graph, CAST_TO_NODE(ret), CAST_TO_NODE(new_assign), 0);
            replace_backward(graph, CAST_TO_NODE(ret), CAST_TO_NODE(new_assign), 0);
            remove_vertex(graph, CAST_TO_NODE(ret));
        }
    }
    
    remove_vertex(graph, vertex);
    remove_vertex(graph, exit);
    
    CAST_TO_DECLARATION(called_func)->use_count--;
    
    return 1;
}


int analyse_inlining(MODULE *module, FUNCTION *func)
{
    GRAPH *graph = func->graph;
    int i;
    int changed = 0;
    
    int original_children = tree_num_children(graph);
    for (i = 0; i < original_children; i++)
        changed |= analyse_vertex(module, func, tree_get_child(graph, i));
    
    return changed;
}
