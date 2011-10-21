
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "compiler.h"


static int is_atomic(EXPRESSION *expr)
{
    if (tree_is_type(expr, EXPR_INTEGER) || tree_is_type(expr, EXPR_STRING) || tree_is_type(expr, EXPR_VARIABLE))
        return 1;
    
    if (tree_is_type(expr, EXPR_CLOSURE))
        return 1;
    
    if (tree_is_type(expr, EXPR_TUPLE))
    {
        int i;
        for (i = 0; i < tree_num_children(expr); i++)
            if (!is_atomic(tree_get_child(expr, i)))
                return 0;
        return 1;
    }
    
    return 0;
}


static int is_short_circuit(EXPRESSION *expr)
{
    return tree_is_type(expr, EXPR_AND) || tree_is_type(expr, EXPR_OR);
}


static int is_simple(EXPRESSION *expr)
{
    if (is_atomic(expr))
        return 1;
    
    if (is_binary_op(expr))
    {
        return is_atomic(tree_get_child(expr, 0)) && is_atomic(tree_get_child(expr, 1));
    }
    
    if (tree_is_type(expr, EXPR_CALL))
    {
        return is_atomic(tree_get_child(expr, 1));
    }
    
    if (tree_is_type(expr, EXPR_TUPLE))
    {
        int i;
        for (i = 0; i < tree_num_children(expr); i++)
            if (!is_simple(tree_get_child(expr, i)))
                return 0;
        return 1;
    }
    
    return 0;
}


static int has_graph(FUNCTION *func)
{
    return func->graph != NULL;
}


static EXPRESSION *atomise_expression(MODULE *module, FUNCTION *func, BLOCK *block, EXPRESSION *expr, STATEMENT *before);


static EXPRESSION *simplify_expression(MODULE *module, FUNCTION *func, BLOCK *block, EXPRESSION *expr, STATEMENT *before)
{
    int i;
    
    int source_line = CAST_TO_AST(expr)->source_line;
    
    if (!has_graph(func) && is_short_circuit(expr))
    {
        TYPE *new_temp_type = CAST_TO_EXPRESSION(tree_get_child(expr, 0))->type;
        EXPRESSION *new_temp = make_new_temp(module, func, new_temp_type, source_line);
        STATEMENT *new_assign = make_assignment(new_temp, tree_get_child(expr, 0), source_line);
        tree_add_before(CAST_TO_NODE(block), CAST_TO_NODE(new_assign), CAST_TO_NODE(before));
        STATEMENT *new_assign2 = make_assignment(new_temp, tree_get_child(expr, 1), source_line);
        EXPRESSION *new_cond = new_temp;
        if (tree_is_type(expr, EXPR_OR))
            new_cond = make_unary_expression(EXPR_NOT, new_cond, source_line);
        STATEMENT *new_if = make_if(new_cond, make_block(NULL, new_assign2, 0), NULL, 0);
        tree_add_before(CAST_TO_NODE(block), CAST_TO_NODE(new_if), CAST_TO_NODE(before));
        return new_temp;
    }
    
    if (has_graph(func) && is_short_circuit(expr))
    {
        GRAPH *graph = func->graph;
        
        EXPRESSION *sub0 = tree_get_child(expr, 0);
        EXPRESSION *sub1 = tree_get_child(expr, 1);
        
        STATEMENT *new_test = make_test(sub0, source_line);
        
        EDGE_TYPE inner_type = tree_is_type(expr, EXPR_OR) ? EDGE_NO : EDGE_YES;
        EDGE_TYPE outer_type = tree_is_type(expr, EXPR_OR) ? EDGE_YES : EDGE_NO;
        
        add_vertex(graph, CAST_TO_NODE(new_test));
        HASH *subhash = get_from_hash(graph->forward, before, sizeof(void *));
        HASH_ITERATOR iter;
        for (hash_iterator(subhash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
        {
            EDGE_TYPE type = (EDGE_TYPE) iter.entry->data;
            if (outer_type & type)
                add_edge(graph, CAST_TO_NODE(new_test), iter.entry->key, type);
            if (inner_type & type)
                inner_type = type;
        }
        inject_before(graph, CAST_TO_NODE(new_test), CAST_TO_NODE(before), inner_type);        
        
        return sub1;
    }
    
    if (is_simple(expr))
        return expr;
    
    if (tree_is_type(expr, EXPR_CALL))
    {
        EXPRESSION *args = CAST_TO_EXPRESSION(tree_get_child(expr, 1));
        args = atomise_expression(module, func, block, args, before);
        tree_get_child(expr, 1) = args;
        return expr;
    }
    
    for (i = 0; i < tree_num_children(expr); i++)
    {
        EXPRESSION *child = tree_get_child(expr, i);
        if (!is_atomic(child))
        {
            TYPE *new_temp_type = CAST_TO_EXPRESSION(child)->type;
            EXPRESSION *new_temp = make_new_temp(module, func, new_temp_type, CAST_TO_AST(child)->source_line);
            STATEMENT *new_assign = make_assignment(new_temp, child, CAST_TO_AST(child)->source_line);
            
            if (has_graph(func))
            {
                GRAPH *graph = func->graph;
                add_vertex(graph, CAST_TO_NODE(new_assign));
                inject_before(graph, CAST_TO_NODE(new_assign), CAST_TO_NODE(before), 0);
            }
            else
                tree_add_before(CAST_TO_NODE(block), CAST_TO_NODE(new_assign), CAST_TO_NODE(before));
            
            tree_get_child(expr, i) = new_temp;
        }
    }
    
    return expr;
}


EXPRESSION *atomise_expression(MODULE *module, FUNCTION *func, BLOCK *block, EXPRESSION *expr, STATEMENT *before)
{
    if (is_atomic(expr))
        return expr;
    
    if (tree_is_type(expr, EXPR_TUPLE))
    {
        EXPRESSION *new_temp = make_empty_tuple(CAST_TO_AST(expr)->source_line);
        int i;
        for (i = 0; i < tree_num_children(expr); i++)
            tree_add_child(new_temp, atomise_expression(module, func, block, tree_get_child(expr, i), before));
        return new_temp;
    }
    
    EXPRESSION *new_temp = make_new_temp(module, func, expr->type, CAST_TO_AST(expr)->source_line);
    STATEMENT *new_assign = make_assignment(new_temp, expr, CAST_TO_AST(expr)->source_line);
    
    if (has_graph(func))
    {
        GRAPH *graph = func->graph;
        add_vertex(graph, CAST_TO_NODE(new_assign));
        inject_before(graph, CAST_TO_NODE(new_assign), CAST_TO_NODE(before), 0);
    }
    else
        tree_add_before(CAST_TO_NODE(block), CAST_TO_NODE(new_assign), CAST_TO_NODE(before));
    
    return new_temp;
}


static int reduce_block(MODULE *module, FUNCTION *func, BLOCK *block);


static STATEMENT *reduce_statement(MODULE *module, FUNCTION *func, BLOCK *block, STATEMENT *stmt)
{
    if (stmt == NULL)
        return stmt;
    
    if (tree_is_type(stmt, STMT_ASSIGN))
    {
        EXPRESSION *expr = tree_get_child(stmt, 1);
        expr = simplify_expression(module, func, block, expr, stmt);
        tree_get_child(stmt, 1) = expr;
    }
    else if (tree_is_type(stmt, STMT_IF))
    {
        EXPRESSION *cond = tree_get_child(stmt, 0);
        cond = simplify_expression(module, func, block, cond, stmt);
        tree_get_child(stmt, 0) = cond;
        reduce_block(module, func, tree_get_child(stmt, 1));
        reduce_block(module, func, tree_get_child(stmt, 2));
    }
    else if (tree_is_type(stmt, STMT_WHILE))
    {
        EXPRESSION *cond = tree_get_child(stmt, 0);
        BLOCK *body = tree_get_child(stmt, 1);
        if (!is_atomic(cond))
        {
            EXPRESSION *old_cond = cond;
            cond = atomise_expression(module, func, block, cond, stmt);
            tree_get_child(stmt, 0) = cond;
            STATEMENT *new_assign = make_assignment(cond, CAST_TO_EXPRESSION(tree_copy(old_cond)), CAST_TO_AST(cond)->source_line);
            tree_add_child(body, new_assign);
        }
        reduce_block(module, func, body);
    }
    else if (tree_is_type(stmt, STMT_RETURN))
    {
        EXPRESSION *expr = tree_get_child(stmt, 0);
        expr = atomise_expression(module, func, block, expr, stmt);
        tree_get_child(stmt, 0) = expr;
    }
    else if (tree_is_type(stmt, STMT_RESTART))
    {
        /* Do nothing. */
    }
    else
        error("Not sure how to reduce statement of type %d\n", tree_type(stmt));
    
    return stmt;
}


static int reduce_block(MODULE *module, FUNCTION *func, BLOCK *block)
{
    int i;
    int size;
    
    if (block == NULL)
        return 0;
    
    size = tree_num_children(block);
    for (i = 0; i < tree_num_children(block); i++)
    {
        STATEMENT *stmt = tree_get_child(block, i);
        
        if (tree_is_type(stmt, STMT_BLOCK))
            reduce_block(module, func, CAST_TO_BLOCK(stmt));
        else
            reduce_statement(module, func, block, stmt);
        
        if (size != tree_num_children(block))
        {
            size = tree_num_children(block);
            i--;
        }
    }
    
    return 1;
}


static int reduce_graph(MODULE *module, FUNCTION *func, GRAPH *graph)
{
    int i;
    int num;
restart:
    num = tree_num_children(graph);
    for (i = 0; i < num; i++)
    {
        NODE *vertex = tree_get_child(graph, i);
        if (!vertex)
            continue;
        
        if (tree_is_type(vertex, STMT_ASSIGN))
        {
            EXPRESSION *expr = tree_get_child(vertex, 1);
            expr = simplify_expression(module, func, tree_get_child(func, 0), expr, CAST_TO_STATEMENT(vertex));
            tree_get_child(vertex, 1) = expr;
        }
        else if (tree_is_type(vertex, STMT_TEST))
        {
            EXPRESSION *expr = tree_get_child(vertex, 0);
            expr = simplify_expression(module, func, tree_get_child(func, 0), expr, CAST_TO_STATEMENT(vertex));
            tree_get_child(vertex, 0) = expr;
        }
        else if (tree_is_type(vertex, STMT_RETURN))
        {
            EXPRESSION *expr = tree_get_child(vertex, 0);
            expr = atomise_expression(module, func, tree_get_child(func, 0), expr, CAST_TO_STATEMENT(vertex));
            tree_get_child(vertex, 0) = expr;
        }
    }
    
    if (num != tree_num_children(graph))
        goto restart;
    
    return 1;
}


int reduce(MODULE *module, FUNCTION *func)
{
    int changed;
    
    if (has_graph(func))
        changed = reduce_graph(module, func, func->graph);
    else
        changed = reduce_block(module, func, tree_get_child(func, 0));
    
    return changed;
}
