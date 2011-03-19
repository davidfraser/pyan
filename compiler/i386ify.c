
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "compiler.h"


static int is_same_var(EXPRESSION *expr1, EXPRESSION *expr2)
{
    if (!tree_is_type(expr1, EXPR_VARIABLE) || !tree_is_type(expr2, EXPR_VARIABLE))
        return 0;
    
    VARIABLE *var1 = CAST_TO_VARIABLE(expr1);
    VARIABLE *var2 = CAST_TO_VARIABLE(expr2);
    if (strcmp(var1->name, var2->name))
        return 0;
    return 1;
}


/**
 * For unary operations, one case is already in i386 form:
 *     a = #a
 * There are two cases which need processing:
 *     a = #k   (should really be handled by constant optimisation)
 *     a = #b
 * These are both translated to a = k/b; a = #a
 */
static int i386ify_unary_operation(MODULE *module, FUNCTION *func, NODE *vertex)
{
    GRAPH *graph = func->graph;
    VARIABLE *dest = tree_get_child(vertex, 0);
    EXPRESSION *expr = tree_get_child(vertex, 1);
    EXPRESSION *arg0 = tree_get_child(expr, 0);
    
    if (is_same_var(CAST_TO_EXPRESSION(dest), arg0))
        return 0;
    
    STATEMENT *new_assign = make_assignment(CAST_TO_EXPRESSION(tree_copy(dest)), arg0);
    tree_get_child(expr, 0) = tree_copy(dest);
    add_vertex(graph, CAST_TO_NODE(new_assign));
    replace_backward(graph, vertex, CAST_TO_NODE(new_assign), 0);
    add_edge(graph, CAST_TO_NODE(new_assign), vertex, 0);
    
    return 1;
}


/**
 * For binary operations, there are two cases that need special treatment:
 *     a = k # a
 *     a = b # a
 * If # is commutable then these can be reversed.  Otherwise they will be
 * translated to t = a; a = k/b # t.
 * After the first phase there are three cases that are already in i386 form:
 *     a = a # a
 *     a = a # b
 *     a = a # k
 * There are five cases that need processing:
 *     a = k # k   (should really be handled by constant optimisation)
 *     a = k # b
 *     a = b # k
 *     a = b # b
 *     a = b # c
 * All are translated to a = (k/b); a = a # (k/b/c)
 */
static int i386ify_binary_operation(MODULE *module, FUNCTION *func, NODE *vertex)
{
    GRAPH *graph = func->graph;
    VARIABLE *dest = tree_get_child(vertex, 0);
    EXPRESSION *expr = tree_get_child(vertex, 1);
    EXPRESSION *arg0 = tree_get_child(expr, 0);
    EXPRESSION *arg1 = tree_get_child(expr, 1);
    
    /* First deal with special cases, where the first argument is not the
       destination but the second argument is. */
    if (!is_same_var(CAST_TO_EXPRESSION(dest), arg0) && is_same_var(CAST_TO_EXPRESSION(dest), arg1))
    {
        if (is_commutable_op(expr))
        {
            tree_get_child(expr, 0) = arg1;
            tree_get_child(expr, 1) = arg0;
        }
        else
        {
            TYPE *new_temp_type = arg1->type;
            EXPRESSION *new_temp = make_new_temp(module, func, new_temp_type);
            STATEMENT *new_assign = make_assignment(new_temp, arg1);
            tree_get_child(expr, 1) = tree_copy(new_temp);
            add_vertex(graph, CAST_TO_NODE(new_assign));
            replace_backward(graph, vertex, CAST_TO_NODE(new_assign), 0);
            add_edge(graph, CAST_TO_NODE(new_assign), vertex, 0);
        }
        
        /* Reset these as the above operations may have changed them, */
        arg0 = tree_get_child(expr, 0);
        arg1 = tree_get_child(expr, 1);
    }
    
    /* If instruction is already in i386 form, we don't need to process it. */
    if (is_same_var(CAST_TO_EXPRESSION(dest), arg0))
        return 0;
    
    /* Otherwise, translate a = b # c to a = b; a = a # c. */
    STATEMENT *new_assign = make_assignment(CAST_TO_EXPRESSION(tree_copy(dest)), arg0);
    tree_get_child(expr, 0) = tree_copy(dest);
    add_vertex(graph, CAST_TO_NODE(new_assign));
    replace_backward(graph, vertex, CAST_TO_NODE(new_assign), 0);
    add_edge(graph, CAST_TO_NODE(new_assign), vertex, 0);
        
    return 1;
}


static int i386ify_assignment(MODULE *module, FUNCTION *func, NODE *vertex)
{
    int changed = 0;
    GRAPH *graph = func->graph;
    VARIABLE *dest = tree_get_child(vertex, 0);
    EXPRESSION *expr = tree_get_child(vertex, 1);
    
    if (is_unary_op(expr))
        changed |= i386ify_unary_operation(module, func, vertex);
    
    if (is_binary_op(expr))
        changed |= i386ify_binary_operation(module, func, vertex);
    
    /* Expand tuple assignments. */
    if (tree_is_type(dest, EXPR_TUPLE) && tree_num_children(dest) >= 1)
    {
        int i;
        NODE *last = NULL;
        if (tree_num_children(dest) != tree_num_children(expr))
            error("Source and destinations have different cardinality!");
        for (i = 0; i < tree_num_children(dest); i++)
        {
            VARIABLE *dest2 = tree_get_child(dest, i);
            VARIABLE *src2 = tree_get_child(expr, i);
            STATEMENT *new_assign = make_assignment(CAST_TO_EXPRESSION(dest2), CAST_TO_EXPRESSION(src2));
            add_vertex(graph, CAST_TO_NODE(new_assign));
            if (last)
                add_edge(graph, last, CAST_TO_NODE(new_assign), 0);
            else
                replace_backward(graph, vertex, CAST_TO_NODE(new_assign), 0);
            last = CAST_TO_NODE(new_assign);
        }
        replace_forward(graph, vertex, last, 0);
        remove_vertex(graph, vertex);
        changed |= 1;
    }
    
    return changed;
}


int i386ify(MODULE *module, FUNCTION *func)
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
            changed |= i386ify_assignment(module, func, vertex);
    }
    
    return changed;
}
