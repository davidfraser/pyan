#include "compiler.h"
#include "dfa.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


static void *create_start_set(void *data, int type)
{
    DAA_SET *set = malloc(sizeof(DAA_SET));
    set->type = type;
    set->set = create_hash(10, key_type_indirect);
    set->first_time = 1;
    return set;
}


static void add_all_vars(FUNCTION *func, HASH *set)
{
    HASH_ITERATOR iter;
    for (hash_iterator(func->table, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
        add_to_hash(set, iter.entry->key, iter.entry->key_len, iter.entry->data);
    
    /*int i;
    for (i = 0; i < tree_num_children(block); i++)
    {
        STATEMENT *stmt = tree_get_child(block, i);
        if (tree_is_type(stmt, STMT_BLOCK))
            add_all_vars(CAST_TO_BLOCK(stmt), set);
    }*/
}


static void *create_default_set(void *data, int type)
{
    DAA_SET *set = malloc(sizeof(DAA_SET));
    set->type = type;
    set->set = create_hash(10, key_type_indirect);
    FUNCTION *func = data;
    add_all_vars(func, set->set);
    set->first_time = 1;
    return set;
}


static void destroy_set(void *setptr)
{
    DAA_SET *set = setptr;
    destroy_hash(set->set);
    free(set);
}


static int update_set(DAA_SET *set, DECLARATION *decl)
{
    if (!set)
        return 0;
    
    if (find_in_hash(set->set, decl->name, strlen(decl->name)))
        return 0;
    

    
    add_to_hash(set->set, decl->name, strlen(decl->name), decl);
    return 1;
}


static int update_output(LIST *output, DECLARATION *decl)
{
    int changed = 0;
    int i;
    
    for (i = 0; i < output->size; i++)
        changed |= update_set(output->items[i], decl);
    
    return changed;
}


static int update_all_destinations(HASH *table, DAA_SET *set, EXPRESSION *expr)
{
    int changed = 0;
    
    if (tree_is_type(expr, EXPR_VARIABLE))
    {
        VARIABLE *dest = CAST_TO_VARIABLE(expr);
        DECLARATION *decl = get_from_hash(table, dest->name, strlen(dest->name));
        if (!decl)
            error("No declaration of variable called '%s'!", dest->name);
        changed |= update_set(set, decl);
    }
    else if (tree_is_type(expr, EXPR_TUPLE))
    {
        int i;
        for (i = 0; i < tree_num_children(expr); i++)
            changed |= update_all_destinations(table, set, tree_get_child(expr, i));
    }
    
    return changed;
}


static int update_output_destinations(HASH *table, LIST *output, EXPRESSION *expr)
{
    int changed = 0;
    int i;
    
    for (i = 0; i < output->size; i++)
        changed |= update_all_destinations(table, output->items[i], expr);
    
    return changed;
}


static int verify(NODE *vertex, LIST *input, LIST *output, void *data);


static int analyse(NODE *vertex, LIST *input, LIST *output, void *data)
{
    int changed = 0;
    int i;
    FUNCTION *func = data;
    HASH *table = func->table;
    
    for (i = 0; i < output->size; i++)
    {
        DAA_SET *out_set = output->items[i];
        if (out_set->first_time)
        {
            changed = 1;
            out_set->first_time = 0;
        }
    }
    
    for (i = 0; i < input->size; i++)
    {
        DAA_SET *in_set = input->items[i];
        if (in_set->first_time)
        {
            in_set = create_default_set(data, in_set->type);
            input->items[i] = in_set;
        }
    }
    
    if (tree_is_type(vertex, STMT_ENTER))
    {
        if (tree_get_child(func, 1))
        {
            int i;
            DECLARATION *args = tree_get_child(func, 1);
            
            for (i = 0; i < tree_num_children(args); i++)
            {
                DECLARATION *arg = tree_get_child(args, i);
                changed |= update_output(output, arg);
            }
        }
        
        return changed;
    }
    else if (tree_is_type(vertex, STMT_JOIN))
    {
        HASH_ITERATOR iter;
        DAA_SET *in = input->items[0];
        for (hash_iterator(in->set, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
        {
            int in_all = 1;
            for (i = 1; i < input->size; i++)
            {
                in = input->items[i];
                if (!find_in_hash(in->set, iter.entry->key, iter.entry->key_len))
                    in_all = 0;
            }
            if (in_all)
                changed |= update_output(output, iter.entry->data);
        }
        return changed;
    }
    else if (tree_is_type(vertex, STMT_ASSIGN) && verify(vertex, input, output, data))
    {
        changed |= update_output_destinations(table, output, tree_get_child(vertex, 0));
    }

    for (i = 0; i < input->size; i++)
    {
        HASH_ITERATOR iter;
        DAA_SET *in = input->items[i];
        for (hash_iterator(in->set, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
            changed |= update_output(output, iter.entry->data);
    }

    return changed;
}


static int is_verifiable_node(NODE *vertex)
{
    return vertex && !tree_is_type(vertex, STMT_PASS) && !tree_is_type(vertex, STMT_JOIN) && !tree_is_type(vertex, STMT_ENTER) && !tree_is_type(vertex, STMT_EXIT);
}


static int verify_expression(EXPRESSION *expr, DAA_SET *set, int num, char *name)
{
    if (tree_is_type(expr, EXPR_INTEGER))
        return 1;
    else if (tree_is_type(expr, EXPR_VARIABLE))
    {
        VARIABLE *var = CAST_TO_VARIABLE(expr);
        if (find_in_hash(set->set, var->name, strlen(var->name)))
            return 1;
        
        fprintf(stderr, "Variable '%s' may not be defined at vertex %d in '%s'\n", var->name, num, name);
    }
    else if (tree_is_type(expr, EXPR_CALL))
        return verify_expression(tree_get_child(expr, 1), set, num, name);
    else
    {
        int i;
        int ok = 1;
        for (i = 0; i < tree_num_children(expr); i++)
            ok &= verify_expression(tree_get_child(expr, i), set, num, name);
        return ok;
    }
    return 0;
}


static int verify(NODE *vertex, LIST *input, LIST *output, void *data)
{
    if (!is_verifiable_node(vertex))
        return 1;
    
    DAA_SET *in = input->items[0];
    
    FUNCTION *func = data;
    GRAPH *graph = func->graph;
    int num = (int) get_from_hash(graph->labels, vertex, sizeof(void *));
    
    if (tree_is_type(vertex, STMT_ASSIGN))
    {
        EXPRESSION *expr = CAST_TO_EXPRESSION(tree_get_child(vertex, 1));
        if (!verify_expression(expr, in, num, CAST_TO_DECLARATION(func)->name))
            return 0;
    }
    else if (tree_is_type(vertex, STMT_RETURN))
    {
        EXPRESSION *expr = CAST_TO_EXPRESSION(tree_get_child(vertex, 0));
        if (!verify_expression(expr, in, num, CAST_TO_DECLARATION(func)->name))
            return 0;
    }
    else if (tree_is_type(vertex, STMT_TEST))
    {
        EXPRESSION *expr = CAST_TO_EXPRESSION(tree_get_child(vertex, 0));
        if (!verify_expression(expr, in, num, CAST_TO_DECLARATION(func)->name))
            return 0;
    }
    
    return 1;
}


int definite_assignment_analysis(MODULE *module, FUNCTION *func)
{
    fprintf(stderr, "Performing definite assigment analysis on '%s'\n", CAST_TO_DECLARATION(func)->name);
    DFA_FUNCTIONS funcs = { create_start_set, create_default_set, destroy_set, analyse, verify };
    DFA *dfa = create_dfa(func, &funcs, func, DFA_FORWARD | DFA_ADD_JOINS);
    run_dfa(dfa);
    destroy_dfa(dfa);
    
    cleanup_graph(func);
    
    return 1;
}
