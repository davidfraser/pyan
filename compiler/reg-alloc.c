#include "compiler.h"
#include "dfa.h"

#include <string.h>
#include <stdio.h>


static void add_interference_edges(GRAPH *interference, HASH *set)
{
    HASH_ITERATOR iter;
    for (hash_iterator(set, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        HASH_ITERATOR iter2;
        for (hash_iterator(set, &iter2); hash_iterator_valid(&iter2); hash_iterator_next(&iter2))
        {
            DECLARATION *var1 = iter.entry->data;
            DECLARATION *var2 = iter2.entry->data;
            if (strcmp(var1->name, var2->name) > 0
                    && find_in_hash(interference->labels, var1, sizeof(void *))
                    && find_in_hash(interference->labels, var2, sizeof(void *)))
            {
                add_edge(interference, CAST_TO_NODE(var1), CAST_TO_NODE(var2), 0);
                add_edge(interference, CAST_TO_NODE(var2), CAST_TO_NODE(var1), EDGE_SYMMETRICAL);
            }
        }
    }
}


static GRAPH *build_interference_graph(FUNCTION *func)
{
    GRAPH *interference = make_graph(func);
    HASH *hash = func->table;
    HASH_ITERATOR iter;
    for (hash_iterator(hash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        if (!strcmp(iter.entry->key, "$parent"))
            continue;
        DECLARATION *decl = iter.entry->data;
        if (!tree_is_type(decl->type, TYPE_INT))
        {
            //tree_print(CAST_TO_NODE(decl), 0);
            //tree_print(CAST_TO_NODE(decl->type), 1);
        }
        add_vertex(interference, CAST_TO_NODE(decl));
    }
    
    DFA *liveness = func->liveness;
    for (hash_iterator(liveness->inputs, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        LIST *list = iter.entry->data;
        int i;
        
        for (i = 0; i < list->size; i++)
        {
            DAA_SET *input = list->items[i];
            HASH *set = input->set;
            add_interference_edges(interference, set);
        }
    }
    
    return interference;
}


static int find_new_colour(GRAPH *graph, DECLARATION *vertex)
{
    int colour = 1;
    HASH *subhash = get_from_hash(graph->forward, vertex, sizeof(void *));
    HASH_ITERATOR iter;
restart:
    for (hash_iterator(subhash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        DECLARATION *vertex2 = iter.entry->key;
        if (vertex2->colour == colour)
        {
            colour++;
            goto restart;
        }
    }
    
    return colour;
}


static void colour_graph_search(GRAPH *graph, DECLARATION *vertex)
{
    HASH *subhash = get_from_hash(graph->forward, vertex, sizeof(void *));
    HASH_ITERATOR iter;
    for (hash_iterator(subhash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        DECLARATION *vertex2 = iter.entry->key;
        if (vertex2->colour == 0)
        {
            vertex2->colour = find_new_colour(graph, vertex2);
            colour_graph_search(graph, vertex2);
        }
        else if (vertex->colour == vertex2->colour)
        {
            printf("conflicting edge between %s and %s!\n", vertex->name, vertex2->name);
        }
    }
}


static void colour_graph(GRAPH *graph)
{
    int i;
    for (i = 0; i < tree_num_children(graph); i++)
    {
        DECLARATION *vertex = tree_get_child(graph, i);
        if (vertex->colour != 0)
            continue;
        
        vertex->colour = 1;
        colour_graph_search(graph, vertex);
    }
}


static void assign_registers(MODULE *module, FUNCTION *func)
{
    HASH *hash = func->table;
    HASH_ITERATOR iter;
    for (hash_iterator(hash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        if (!strcmp(iter.entry->key, "$parent"))
            continue;
        DECLARATION *decl = iter.entry->data;
        if (!tree_is_type(decl->type, TYPE_INT))
        {
            //tree_print(CAST_TO_NODE(decl), 0);
            //tree_print(CAST_TO_NODE(decl->type), 1);
        }
        if (decl->colour > module->max_registers)
        {
            fprintf(stderr, "Variable %s spilled!\n", decl->name);
            decl->colour = 0;
        }
    }
}


static int graph_colouring(MODULE *module, FUNCTION *func)
{
    GRAPH *interference = build_interference_graph(func);
    
    colour_graph(interference);
    assign_registers(module, func);
    
    //print_graph(interference, "interference", NULL);
    //print_graph(func->graph, func->decl.name, func->liveness);
    
    return 1;
}


static int is_register(EXPRESSION *expr)
{
    if (!tree_is_type(expr, EXPR_VARIABLE))
        return 0;
    VARIABLE *var = CAST_TO_VARIABLE(expr);
    DECLARATION *decl = var->decl;
    return decl->colour != 0;
}


static void replace_child_with_temporary(MODULE *module, FUNCTION *func, NODE *vertex, EXPRESSION *expr, int child_num)
{
    GRAPH *graph = func->graph;
    EXPRESSION *child = tree_get_child(expr, child_num);
    TYPE *new_temp_type = child->type;
    EXPRESSION *new_temp = make_new_temp(module, func, new_temp_type);
    STATEMENT *new_assign = make_assignment(new_temp, child);
    tree_get_child(expr, child_num) = tree_copy(new_temp);
    add_vertex(graph, CAST_TO_NODE(new_assign));
    replace_backward(graph, vertex, CAST_TO_NODE(new_assign), 0);
    add_edge(graph, CAST_TO_NODE(new_assign), vertex, 0);
}


static int check_assignment(MODULE *module, FUNCTION *func, NODE *vertex)
{
    VARIABLE *dest = tree_get_child(vertex, 0);
    EXPRESSION *expr = tree_get_child(vertex, 1);
    
    if (!is_binary_op(expr))
        return 0;
    
    EXPRESSION *arg1 = tree_get_child(expr, 1);
    if (!tree_is_type(arg1, EXPR_VARIABLE))
        return 0;

    VARIABLE *src = CAST_TO_VARIABLE(arg1);
    if (is_register(CAST_TO_EXPRESSION(dest)) || is_register(CAST_TO_EXPRESSION(src)))
        return 0;
    
    replace_child_with_temporary(module, func, vertex, expr, 1);
    
    return 1;
}


static int check_test(MODULE *module, FUNCTION *func, NODE *vertex)
{
    EXPRESSION *expr = tree_get_child(vertex, 0);
    
    if (is_binary_op(expr))
    {
        EXPRESSION *expr0 = tree_get_child(expr, 0);
        EXPRESSION *expr1 = tree_get_child(expr, 1);
        
        //TODO tests can be optimised a bit i think
        if (is_register(expr1) /*|| is_register(expr1)*/)
            return 0;

        int child_num = 1;
        replace_child_with_temporary(module, func, vertex, expr, child_num);
        return 1;
    }
    
    return 0;
}


static int check_validity(MODULE *module, FUNCTION *func)
{
    GRAPH *graph = func->graph;
    int changed;
    
    int i;
    for (i = 0; i < tree_num_children(graph); i++)
    {
        NODE *vertex = tree_get_child(graph, i);
        if (!vertex)
            continue;
        
        if (tree_is_type(vertex, STMT_ASSIGN))
            changed |= check_assignment(module, func, vertex);
        if (tree_is_type(vertex, STMT_TEST))
            changed |= check_test(module, func, vertex);
    }
    
    return changed;
}


extern int liveness_analysis(MODULE *mod, FUNCTION *func);


/** Allocate a register to each variable
 *
 * ALGORITHM
 * Build an interference graph with a vertex for each variable in the function.
 * Using the liveness information, add an edge between two vertices if they
 * are both members of the same set somewhere in the liveness result.
 * Colour the vertices of the graph, and assign a register to each colour.
 * If a register is not available for a vertex, that vertex is marked as not
 * in a register.  If a statement using it is not in i386 form, then the register
 * will be replaced by a temporary in that statement, and liveness analysis
 * and register colouring will be rerun.
 */
int register_allocation(MODULE *module, FUNCTION *func)
{
    int changed = 0;
    do
    {
        liveness_analysis(module, func);
        graph_colouring(module, func);
        changed = !check_validity(module, func);
    }
    while (changed);
    
    return 1;
}
