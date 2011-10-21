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


static void *create_default_set(void *data, int type)
{
    DAA_SET *set = malloc(sizeof(DAA_SET));
    set->type = type;
    set->set = create_hash(10, key_type_indirect);
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


static int update_set2(DAA_SET *set, DECLARATION *decl)
{
    if (!set)
        return 0;
    
    if (find_in_hash(set->set, decl->name, strlen(decl->name)))
    {
        remove_from_hash(set->set, decl->name, strlen(decl->name));
        return 1;
    }
    
    return 0;
}


static int update_all_destinations(HASH *table, DAA_SET *set, EXPRESSION *expr)
{
    int changed = 0;
    
    if (tree_is_type(expr, EXPR_VARIABLE))
    {
        VARIABLE *dest = CAST_TO_VARIABLE(expr);
        DECLARATION *decl = get_from_hash(table, dest->name, strlen(dest->name));
        changed |= update_set2(set, decl);
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


static int update_all_sources(HASH *table, DAA_SET *set, EXPRESSION *expr)
{
    int changed = 0;
    
    if (tree_is_type(expr, EXPR_VARIABLE))
    {
        VARIABLE *dest = CAST_TO_VARIABLE(expr);
        DECLARATION *decl = dest->decl; /*get_from_hash(table, dest->name, strlen(dest->name));*/
        changed |= update_set(set, decl);
    }
    else if (tree_is_type(expr, EXPR_CALL))
    {
        changed |= update_all_sources(table, set, tree_get_child(expr, 1));
    }
    else
    {
        int i;
        for (i = 0; i < tree_num_children(expr); i++)
            changed |= update_all_sources(table, set, tree_get_child(expr, i));
    }
    
    return changed;
}


static int update_output_sources(HASH *table, LIST *output, EXPRESSION *expr)
{
    int changed = 0;
    int i;
    
    for (i = 0; i < output->size; i++)
        changed |= update_all_sources(table, output->items[i], expr);
    
    return changed;
}


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
    
    if (tree_is_type(vertex, STMT_ENTER) || tree_is_type(vertex, STMT_EXIT))
    {
        return changed;
    }
    else if (tree_is_type(vertex, STMT_JOIN))
    {
        for (i = 0; i < input->size; i++)
        {
            DAA_SET *in = input->items[i];
            HASH_ITERATOR iter;
            for (hash_iterator(in->set, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
                changed |= update_output(output, iter.entry->data);
        }
        return changed;
    }

    for (i = 0; i < input->size; i++)
    {
        HASH_ITERATOR iter;
        DAA_SET *in = input->items[i];
        for (hash_iterator(in->set, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
            changed |= update_output(output, iter.entry->data);
    }
    
    if (tree_is_type(vertex, STMT_RETURN) || tree_is_type(vertex, STMT_TEST))
    {
        changed |= update_output_sources(table, output, tree_get_child(vertex, 0));
    }
    else if (tree_is_type(vertex, STMT_ASSIGN))
    {
        changed |= update_output_destinations(table, output, tree_get_child(vertex, 0));
        changed |= update_output_sources(table, output, tree_get_child(vertex, 1));
    }

    return changed;
}


static int verify(NODE *vertex, LIST *input, LIST *output, void *data)
{
    //TODO
    return 1;
}


int liveness_analysis(MODULE *module, FUNCTION *func)
{
    fprintf(stderr, "Performing liveness analysis on '%s'\n", CAST_TO_DECLARATION(func)->name);
    DFA_FUNCTIONS funcs = { create_start_set, create_default_set, destroy_set, analyse, verify };
    DFA *dfa = create_dfa(func, &funcs, func, DFA_BACKWARD);
    run_dfa(dfa);
    //destroy_dfa(dfa);
    func->liveness = dfa;
    
    //print_graph(func->graph, func->decl.name, dfa);
    
    return 1;
}
