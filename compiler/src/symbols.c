#include "compiler.h"

#include <string.h>
#include <stdio.h>


static int analyse_block(MODULE *module, HASH *table, BLOCK *block, int depth);


static int analyse_expression(MODULE *module, HASH *table, EXPRESSION *expr, int depth)
{
    int changed = 0;
    
    if (!expr)
        return 0;
    
    if (tree_is_type(expr, EXPR_VARIABLE))
    {
        VARIABLE *var = CAST_TO_VARIABLE(expr);
        if (!find_in_hash(table, var->name, strlen(var->name)) && !find_in_hash(module->table, var->name, strlen(var->name)))
        {
            DECLARATION *decl = CAST_TO_DECLARATION(tree_copy(var->decl));
            decl->flags |= DECL_ENCLOSED;
            var->decl = decl;
            add_to_hash(table, decl->name, strlen(decl->name), decl);           
        }
        
        DECLARATION *decl2 = get_from_hash(table, var->name, strlen(var->name));
        if (decl2 && decl2 != var->decl)
        {
            var->decl = decl2;
        }
    }

    int i;
    for (i = 0; i < tree_num_children(expr); i++)
        changed |= analyse_expression(module, table, tree_get_child(expr, i), depth);
    
    return changed;
}


static int analyse_block(MODULE *module, HASH *table, BLOCK *block, int depth)
{
    int i;
    int changed = 0;
    
    if (!block)
        return 0;
    
    /* Copy symbol table. */
    HASH_ITERATOR iter;
    HASH *src_table = block->table;
    
    for (hash_iterator(src_table, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        if (!strcmp(iter.entry->key, "$parent"))
            continue;
        
        char next_name[100];
        sprintf(next_name, "%s", (char *) iter.entry->key);
    
        char *str = add_string(module, next_name, strlen(next_name));
        
        DECLARATION *decl = iter.entry->data;
        add_to_hash(table, str, strlen(str), decl);
        decl->depth = depth;
    }
    
    /* Find and process subblocks. */
    for (i = 0; i < tree_num_children(block); i++)
    {
        STATEMENT *stmt = tree_get_child(block, i);
        if (tree_is_type(stmt, STMT_BLOCK))
        {
            changed |= analyse_block(module, table, CAST_TO_BLOCK(stmt), depth);
        }
        else if (tree_is_type(stmt, STMT_IF))
        {
            changed |= analyse_expression(module, table, tree_get_child(stmt, 0), depth);
            changed |= analyse_block(module, table, tree_get_child(stmt, 1), depth);
            changed |= analyse_block(module, table, tree_get_child(stmt, 2), depth);
        }
        else if (tree_is_type(stmt, STMT_WHILE))
        {
            changed |= analyse_expression(module, table, tree_get_child(stmt, 0), depth);
            changed |= analyse_block(module, table, tree_get_child(stmt, 1), depth);
        }
        else if (tree_is_type(stmt, STMT_RETURN))
        {
            changed |= analyse_expression(module, table, tree_get_child(stmt, 0), depth);
        }
        else if (tree_is_type(stmt, STMT_ASSIGN))
        {
            changed |= analyse_expression(module, table, tree_get_child(stmt, 1), depth);
        }
    }
    
    return changed;
}


/**
 * Find all local symbols and move them into a table per function (or closure).
 */
int analyse_symbols(MODULE *mod, FUNCTION *func)
{
    BLOCK *body = tree_get_child(func, 0);
    func->table = create_hash(10, key_type_copyable);
    fprintf(stderr, "Analysing symbols in '%s'\n", func->decl.name);
    return analyse_block(mod, func->table, body, 1);
}
