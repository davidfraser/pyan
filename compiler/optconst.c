/*
 * optconst.c - Implementation of constant-folding optimisation.
 *
 * Copyright (C) 2003, Edmund Horner.
 */

#include "compiler.h"


static EXPRESSION *optimise_expression(MODULE *module, FUNCTION *func, EXPRESSION *expr);

static EXPRESSION *optimise_binary_expression(MODULE *module, FUNCTION *func, EXPRESSION *expr)
{
    EXPRESSION *expr0 = tree_get_child(expr, 0);
    EXPRESSION *expr1 = tree_get_child(expr, 1);
    
    if (!tree_is_type(expr0, EXPR_INTEGER) || !tree_is_type(expr1, EXPR_INTEGER))
        return expr;

    int v = evaluate_binary_op(expr);
    
    fprintf(stderr, "optimised %s operation!\n", tree_get_name(expr));

    EXPRESSION *e = make_integer_direct(v);
    return e;    
}


EXPRESSION *optimise_expression(MODULE *module, FUNCTION *func, EXPRESSION *expr)
{
    int i;
    
    for (i = 0; i < tree_num_children(expr); i++)
    {
        NODE *subexpr = tree_get_child(expr, i);
        subexpr = optimise_expression(module, func, subexpr);
        tree_get_child(expr, i) = subexpr;
    }    
    
    if (is_binary_op(expr))
    {
        expr = optimise_binary_expression(module, func, expr);
    }
    return expr;
}


/*
 * ALGORITHM
 *
 * For each expression in the program with all-constant subexpressions,
 * evaluate and replace with the constant result.
 */
int optimise_constant_folding(MODULE *module, FUNCTION *func)
{
    int changed;
    int i;
    
    for (i = 0; i < tree_num_children(func->graph); i++)
    {
        NODE *vertex = tree_get_child(func->graph, i);
        if (!vertex)
            continue;
        
        if (tree_is_type(vertex, STMT_ASSIGN))
        {
            EXPRESSION *expr = tree_get_child(vertex, 1);
            expr = optimise_expression(module, func, expr);
            tree_get_child(vertex, 1) = expr;
        }
        else if (tree_is_type(vertex, STMT_TEST))
        {
            EXPRESSION *expr = tree_get_child(vertex, 0);
            expr = optimise_expression(module, func, expr);
            tree_get_child(vertex, 0) = expr;
        }
        else if (tree_is_type(vertex, STMT_RETURN))
        {
            EXPRESSION *expr = tree_get_child(vertex, 0);
            expr = optimise_expression(module, func, expr);
            tree_get_child(vertex, 0) = expr;
        }
	}
	
    return changed;
};
