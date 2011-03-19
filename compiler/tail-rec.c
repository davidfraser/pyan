#include "compiler.h"

#include <string.h>
#include <stdio.h>


static int analyse_block(FUNCTION *func, BLOCK *block)
{
    int i;
    int changed = 0;
    
    if (!block)
        return 0;
    
    for (i = 0; i < tree_num_children(block); i++)
    {
        STATEMENT *stmt = tree_get_child(block, i);
        if (tree_is_type(stmt, STMT_BLOCK))
        {
            changed |= analyse_block(func, CAST_TO_BLOCK(stmt));
        }
        else if (tree_is_type(stmt, STMT_IF))
        {
            changed |= analyse_block(func, CAST_TO_BLOCK(tree_get_child(stmt, 1)));
            changed |= analyse_block(func, CAST_TO_BLOCK(tree_get_child(stmt, 2)));
        }
        else if (tree_is_type(stmt, STMT_WHILE))
        {
            changed |= analyse_block(func, CAST_TO_BLOCK(tree_get_child(stmt, 1)));
        }
        else if (tree_is_type(stmt, STMT_RETURN))
        {
            EXPRESSION *expr = tree_get_child(stmt, 0);
            if (!tree_is_type(expr, EXPR_CALL))
                continue;
            VARIABLE *fvar = tree_get_child(expr, 0);
            if (strcmp(fvar->name, CAST_TO_DECLARATION(func)->name))
                continue;
            EXPRESSION *args = tree_get_child(expr, 1);
            STATEMENT *new_assign = make_assignment(get_input_tuple(func), args);
            STATEMENT *new_restart = make_restart();
            list_insert_before(block->node.children, new_assign, stmt);
            block->node.children->items[i+1] = new_restart;
            ((DECLARATION *) func)->use_count--;
            fprintf(stderr, "Tail call in '%s' optimised\n", CAST_TO_DECLARATION(func)->name);        
        }
    }
    
    return changed;
}


/**
 * Discover and process tail-recursive calls.  A tail-recursive call in f(x) is of the form "return f(y);" and can be 
 * replaced by "x = y; restart;".
 */
int analyse_tail_recursion(MODULE *mod, FUNCTION *func)
{
    BLOCK *body = tree_get_child(func, 0);
    return analyse_block(func, body);
}
