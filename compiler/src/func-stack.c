#include "compiler.h"

#include <string.h>


int analyse_function_size(MODULE *module, FUNCTION *func)
{
    int i;
    
    /* The input size is the size of the input type. */
    TYPE *input_type = tree_get_child(func, 1);
    func->input_size = input_type ? input_type->size : 0;
    
    /* The stack size is the size of arguments plus all local variables, in that order. */
    func->stack_size = 0;
    if (input_type)
        for (i = 0; i < tree_num_children(input_type); i++)
        {
            DECLARATION *decl = tree_get_child(input_type, i);
            decl->stack_position = func->stack_size;
            func->stack_size += 4;
        }
    
    HASH *table = func->table;
    HASH_ITERATOR iter;
    for (hash_iterator(table, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        if (!strcmp(iter.entry->key, "$parent"))
            continue;
        
        DECLARATION *decl = iter.entry->data;
        
        if (decl->flags & DECL_ARGUMENT)
            continue;
        
        decl->stack_position = func->stack_size;
        func->stack_size += 4;
    }
    
    /* The output size is the size of the output type. */
    TYPE *output_type = tree_get_child(CAST_TO_DECLARATION(func)->type, 1);
    func->output_size = output_type->size;
    
    return 1;
}
