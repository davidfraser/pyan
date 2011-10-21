#include "compiler.h"

#include <string.h>
#include <stdio.h>


static DECLARATION *build_closure_args(FUNCTION *closure)
{
    DECLARATION *args = CAST_TO_DECLARATION(tree_create_node(DEF_ARGS));
    DECLARATION *fun_args = CAST_TO_DECLARATION(tree_create_node(DEF_ARGS));
    
    HASH_ITERATOR iter;
    for (hash_iterator(closure->table, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        DECLARATION *decl = iter.entry->data;
        if (decl->flags & DECL_ENCLOSED)
        {
            decl->flags |= DECL_ARGUMENT;
            tree_add_child(fun_args, decl);
            tree_add_child(args, decl);
        }
    }
    
    int i;
    for (i = 0; i < tree_num_children(tree_get_child(closure, 1)); i++)
    {
        DECLARATION *decl = tree_get_child(tree_get_child(closure, 1), i);
        tree_add_child(fun_args, decl);
    }
    tree_get_child(closure, 1) = fun_args;
    
    return args;
}


static int analyse_block(MODULE *module, FUNCTION *func, BLOCK *block);


static EXPRESSION *analyse_expression(MODULE *module, FUNCTION *func, EXPRESSION *expr)
{
    if (!expr)
        return expr;
    
    if (tree_is_type(expr, EXPR_CLOSURE))
    {
        FUNCTION *closure = tree_get_child(expr, 0);
        DECLARATION *args = build_closure_args(closure);
        int source_line = CAST_TO_AST(expr)->source_line;
        
        /* Replace closure expression with call expression. */
        VARIABLE *fvar = CAST_TO_VARIABLE(make_variable("make_closure", source_line));
        fvar->super.type = closure->decl.type;
        EXPRESSION *closure_args = CAST_TO_EXPRESSION(tree_create_node(EXPR_TUPLE));
        tree_add_child(closure_args, make_integer_direct(4 * tree_num_children(args), source_line));
        int i;
        for (i = 0; i < tree_num_children(args); i++)
        {
            DECLARATION *arg = tree_get_child(args, i);
            VARIABLE *mcvar = CAST_TO_VARIABLE(make_variable(arg->name, source_line));
            mcvar->decl = arg;
            tree_add_child(closure_args, mcvar);
        }
        VARIABLE *clos_var = CAST_TO_VARIABLE(make_variable(closure->decl.name, source_line));
        clos_var->decl = CAST_TO_DECLARATION(closure);
        tree_add_child(closure_args, clos_var);
        EXPRESSION *new_expr = CAST_TO_EXPRESSION(make_call(CAST_TO_EXPRESSION(fvar), closure_args, source_line));
        closure->decl.use_count++;
        return new_expr;
    }
    else
    {
        int i;
        for (i = 0; i < tree_num_children(expr); i++)
        {
            tree_get_child(expr, i) = analyse_expression(module, func, tree_get_child(expr, i));
        }
    }
    
    return expr;
}


#define ANALYZE_EXPR(expr) do { \
    EXPRESSION *new_expr = analyse_expression(module, func, expr); \
    if (expr != new_expr) { expr = new_expr; changed |= 1; } \
} while (0)


static int analyse_block(MODULE *module, FUNCTION *func, BLOCK *block)
{
    int i;
    int changed = 0;
    
    if (!block)
        return 0;
    
    /* Find and process closures. */
    for (i = 0; i < tree_num_children(block); i++)
    {
        STATEMENT *stmt = tree_get_child(block, i);
        if (tree_is_type(stmt, STMT_BLOCK))
        {
            changed |= analyse_block(module, func, CAST_TO_BLOCK(stmt));
        }
        else if (tree_is_type(stmt, STMT_IF))
        {
            ANALYZE_EXPR(tree_get_child(stmt, 0));
            changed |= analyse_block(module, func, CAST_TO_BLOCK(tree_get_child(stmt, 1)));
            changed |= analyse_block(module, func, CAST_TO_BLOCK(tree_get_child(stmt, 2)));
        }
        else if (tree_is_type(stmt, STMT_WHILE))
        {
            ANALYZE_EXPR(tree_get_child(stmt, 0));
            changed |= analyse_block(module, func, CAST_TO_BLOCK(tree_get_child(stmt, 1)));
        }
        else if (tree_is_type(stmt, STMT_RETURN))
        {
            ANALYZE_EXPR(tree_get_child(stmt, 0));
        }
        else if (tree_is_type(stmt, STMT_ASSIGN))
        {
            ANALYZE_EXPR(tree_get_child(stmt, 1));
        }
    }
    
    return changed;
}


/**
 * Find all local symbols and move them into a table per function (or closure).
 */
int process_closures(MODULE *mod, FUNCTION *func)
{
    BLOCK *body = tree_get_child(func, 0);
    return analyse_block(mod, func, body);
}
