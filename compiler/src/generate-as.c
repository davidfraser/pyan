#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"


static int label_offset = 0;
static int label_max = 0;

#ifdef __CYGWIN__
static int use_underscore = 1;
#else
static int use_underscore = 0;
#endif


static int safe_division = 0;


static LIST *string_queue;


static MODULE *the_module;  //TODO no global variables!


int queue_string(char *str)
{
    int num = string_queue->size;
    list_append(string_queue, str);
    return num;
}


static int translate_label(int label)
{
    if (label > label_max)
        label_max = label;
    return label_offset + label;
}


static char *get_reg_name(int colour)
{
    switch (colour)
    {
        case 1:
            return "%eax";
        case 2:
            return "%ebx";
        case 3:
            return "%ecx";
        case 4:
            return "%edx";
        case 5:
            return "%esi";
        case 6:
            return "%edi";
        default:
            return "?reg?";
    }
}


static void get_decl_location(FUNCTION *func, DECLARATION *decl, char *buffer, int from_memory)
{
    if (tree_is_type(decl, DEF_FUNCTION))
    {
        sprintf(buffer, "$%s", decl->name);
        return;
    }
    
    if (decl->colour != 0 && !from_memory)
    {
        sprintf(buffer, "%s", get_reg_name(decl->colour));
        return;
    }
    
    int offset = func->input_size - decl->stack_position - 4;
    if (decl->flags & DECL_ARGUMENT)
        offset = 8 + decl->stack_position;
    if (decl->flags & DECL_STATIC)
        offset = 99;  //TODO
    sprintf(buffer, "%d(%%ebp)", offset);
}


static void get_location(FUNCTION *func, EXPRESSION *expr, char *buffer)
{
    if (tree_is_type(expr, EXPR_INTEGER))
    {
        INTEGER *val = CAST_TO_INTEGER(expr);
        sprintf(buffer, "$%d", val->value);
        return;
    }
    else if (!tree_is_type(expr, EXPR_VARIABLE))
        error("Can't get location of something that's not a variable or a constant!\n");
    
    VARIABLE *var = CAST_TO_VARIABLE(expr);
    char *name = var->name;
    DECLARATION *decl = get_from_hash(func->table, name, strlen(name));
    if (!decl)
        decl = get_from_hash(the_module->table, name, strlen(name));
    if (!decl)
        error("Unable to look up declaration of '%s' in symbol table!", name);
    
    get_decl_location(func, decl, buffer, 0);
}


static void emit_load(FUNCTION *func, EXPRESSION *expr, char *regname)
{
    if (tree_is_type(expr, EXPR_VARIABLE))
    {
        VARIABLE *var = CAST_TO_VARIABLE(expr);
        char loc[100];
        get_location(func, CAST_TO_EXPRESSION(var), loc);
        printf("    movl %s, %s\n", loc, regname);
    }
    else if (tree_is_type(expr, EXPR_INTEGER))
    {
        INTEGER *val = CAST_TO_INTEGER(expr);
        printf("    movl $%d, %s\n", val->value, regname);
    }
    else if (tree_is_type(expr, EXPR_STRING))
    {
        STRING *val = CAST_TO_STRING(expr);
        printf("    movl $.LC%d, %s\n", queue_string(val->value), regname);
    }
    else
    {
        printf("   # load\n");
        tree_print(CAST_TO_NODE(expr), 5);
    }
}


static void emit_load_address(FUNCTION *func, EXPRESSION *expr, char *regname)
{
    if (tree_is_type(expr, EXPR_VARIABLE))
    {
        VARIABLE *var = CAST_TO_VARIABLE(expr);
        char loc[100];
        get_location(func, var, loc);
        printf("    leal %s, %s\n", loc, regname);
    }
    else
    {
        printf("   # load address\n");
        tree_print(CAST_TO_NODE(expr), 5);
    }
}


static void emit_store(FUNCTION *func, EXPRESSION *expr, char *regname)
{
    if (tree_is_type(expr, EXPR_VARIABLE))
    {
        VARIABLE *var = CAST_TO_VARIABLE(expr);
        char loc[100];
        get_location(func, var, loc);
        printf("    movl %s, %s\n", regname, loc);
    }
    else
    {
        printf("   # store\n");
        tree_print(CAST_TO_NODE(expr), 5);
    }
}


static void emit_comment(NODE *vertex, void *data)
{
    FUNCTION *func = data;
    GRAPH *graph = func->graph;
    int label = (int) get_from_hash(graph->labels, vertex, sizeof(void *));
    printf("#%d ", label);
    print_expression(vertex, NULL);
    printf("\n");
}


static void emit_enter(NODE *vertex, void *data)
{
    FUNCTION *func = data;
    int i;
    
    char *prefix = use_underscore ? "_" : "";
    if (CAST_TO_DECLARATION(func)->flags & DECL_PUBLIC)
        printf(".globl %s%s\n", prefix, CAST_TO_DECLARATION(func)->name);
    printf("%s%s:\n", prefix, CAST_TO_DECLARATION(func)->name);
    printf("    pushl %%ebp\n");
    printf("    movl %%esp, %%ebp\n");
    printf("    pushl %%ebx\n");
    printf("    subl $%d, %%esp\n", func->stack_size - func->input_size);
    
    /* Load arguments into their registers. */
    HASH *table = func->table;
    HASH_ITERATOR iter;
    for (hash_iterator(table, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        if (!strcmp(iter.entry->key, "$parent"))
            continue;
        
        DECLARATION *decl = iter.entry->data;
        
        if (!(decl->flags & DECL_ARGUMENT) || decl->colour == 0)
            continue;
        
        char loc[100];
        get_decl_location(func, decl, loc, 1);
        printf("    movl %s, %s\n", loc, get_reg_name(decl->colour));
    }
}


static void emit_exit(NODE *vertex, void *data)
{
    FUNCTION *func = data;
    printf("    popl %%ebx\n");
    printf("    leave\n");
    printf("    ret\n");
}


static void emit_end(void *data)
{
    printf("\n");
}


static void emit_label(int label, void *data)
{
    printf("L%d:\n", translate_label(label));
}


static void emit_jump(int label, void *data)
{
    printf("    jmp L%d\n", translate_label(label));
}


static void emit_return(NODE *vertex, void *data)
{
    FUNCTION *func = data;
    EXPRESSION *expr = tree_get_child(vertex, 0);
    //if (tree_is_type(expr, EXPR_VARIABLE))
    {
        emit_load(func, expr, "%eax");
    }
    //else
    //    printf("    # movl ?, %%eax\n");
}


static char *get_op_name(NODE *expr)
{
    NODE_TYPE type = expr->type;
    switch (type)
    {
        case EXPR_SUM:
            return "addl";
        case EXPR_PRODUCT:
            return "imull";
        case EXPR_DIFFERENCE:
            return "subl";
        case EXPR_RATIO:
            return "idivl";
        case EXPR_NEGATION:
            return "negl";
        default:
            return "?op?";
    }
}


static void emit_assign(NODE *vertex, void *data)
{
    FUNCTION *func = data;
    VARIABLE *dest = tree_get_child(vertex, 0);
    EXPRESSION *expr = tree_get_child(vertex, 1);
    if (tree_is_type(expr, EXPR_INTEGER))
    {
        INTEGER *val = CAST_TO_INTEGER(expr);
        char loc[100];
        get_location(func, dest, loc);
        printf("    movl $%d, %s\n", val->value, loc);
    }
    else if (tree_is_type(expr, EXPR_VARIABLE))
    {
        char loc1[100], loc2[100];
        get_location(func, dest, loc1);
        get_location(func, expr, loc2);
        if (strcmp(loc1, loc2))
            printf("    movl %s, %s\n", loc2, loc1);
    }
    else if (tree_is_type(expr, EXPR_RATIO))
    {
        EXPRESSION *arg0 = tree_get_child(expr, 0);
        EXPRESSION *arg1 = tree_get_child(expr, 1);
        
        GRAPH *graph = func->graph;
        int label = (int) get_from_hash(graph->labels, vertex, sizeof(void *));
        
        if (safe_division)
        {
            emit_load(func, arg0, "%eax");
            printf("    movl $0, %%ebx\n");
            printf("    cmpl %%ebx, %%eax\n");
            printf("    jne L%dZ1:\n", translate_label(label));
            printf("    movl $%d, %%eax\n", 1 << 31);
            printf("    jmp L%dZ2\n", translate_label(label));
            printf("L%dZ1:\n", translate_label(label));
        }
        
        emit_load(func, arg0, "%eax");
        printf("    cltd\n");
        emit_load(func, arg1, "%ebx");
        printf("    idivl %%ebx\n");
        if (safe_division)
        {
            printf("L%dZ2:\n", translate_label(label));
        }
        emit_store(func, CAST_TO_EXPRESSION(dest), "%eax");
    }
    else if (is_binary_op(expr))
    {
        EXPRESSION *src = tree_get_child(expr, 1);
        char *op = get_op_name(CAST_TO_NODE(expr));
        char loc1[100], loc2[100];
        get_location(func, dest, loc1);
        get_location(func, src, loc2);
        printf("    %s %s, %s\n", op, loc2, loc1);
    }
    else if (is_unary_op(expr))
    {
        EXPRESSION *arg0 = tree_get_child(expr, 0);
        char loc[100];
        get_location(func, arg0, loc);
        char *op = get_op_name(CAST_TO_NODE(expr));
        printf("    %s %s\n", op, loc);
    }
    else if (tree_is_type(expr, EXPR_CALL))
    {
        VARIABLE *fun = CAST_TO_VARIABLE(tree_get_child(expr, 0));
        EXPRESSION *args = CAST_TO_EXPRESSION(tree_get_child(expr, 1));
        if (tree_is_type(args, EXPR_INTEGER) || tree_is_type(args, EXPR_STRING) || tree_is_type(args, EXPR_VARIABLE))
        {
            emit_load(func, args, "%eax");
            printf("    pushl %%eax\n");
        }
        else if (tree_is_type(args, EXPR_TUPLE))
        {
            int i;
            for (i = tree_num_children(args) - 1; i >= 0; i--)
            {
                emit_load(func, tree_get_child(args, i), "%eax");
                printf("    pushl %%eax\n");
            }
        }
        else
            printf("   # push args\n");
        char *prefix = use_underscore ? "_" : "";
        if (!fun->decl || fun->decl->flags & DECL_STATIC)
            printf("    call %s%s\n", prefix, fun->name);
        else
        {
            emit_load(func, CAST_TO_EXPRESSION(fun), "%eax");
            printf("    call *%%eax\n");
        }
        if (dest)
            emit_store(func, CAST_TO_EXPRESSION(dest), "%eax");
    }
    else
    {
        printf("   # statement?\n");
    }
}


static char *get_jump_op_name(NODE *expr, EDGE_TYPE branch_type)
{
    NODE_TYPE type = expr->type;
    
    if (branch_type & EDGE_NO)
        type = negate_comparison(type);
    
    switch (type)
    {
        case EXPR_GT:
            return "jg";
        case EXPR_LT:
            return "jl";
        case EXPR_GEQ:
            return "jge";
        case EXPR_LEQ:
            return "jle";
        case EXPR_EQ:
            return "jz";
        case EXPR_NEQ:
            return "jnz";
        default:
            return "?jump?";
    }
}


static void emit_test(NODE *vertex, EDGE_TYPE branch_type, int label, void *data)
{
    FUNCTION *func = data;
    
    EXPRESSION *expr = tree_get_child(vertex, 0);
    
    if (!is_comparison_op(expr))
    {
        printf("   # test\n");
        return;
    }
    
    EXPRESSION *expr0 = tree_get_child(expr, 0);
    EXPRESSION *expr1 = tree_get_child(expr, 1);
    
    char loc1[100], loc2[100];
    get_location(func, expr0, loc1);
    get_location(func, expr1, loc2);
    
    printf("    cmpl %s, %s\n", loc2, loc1);
    
    char *jump_op = get_jump_op_name(CAST_TO_NODE(expr), branch_type);
    printf("    %s L%d\n", jump_op, translate_label(label));

    /*if (tree_is_type(expr, EXPR_LT) && tree_is_type(expr1, EXPR_INTEGER))
    {
        INTEGER *val = CAST_TO_INTEGER(expr1);
        if (val->value == 0)
        {
            VARIABLE *var = expr0;
            printf("    movl %s, %%eax\n", var->name);
            printf("    js L%d\n", label);
        }
        else
            printf("    # branch lt\n");
    }
    else
        printf("    # branch\n");*/
}


int generate_as(MODULE *module)
{
    int i;
    
    EMIT_FUNCTIONS functions = {
        emit_comment,
        emit_enter,
        emit_exit,
        emit_end,
        emit_label,
        emit_jump,
        emit_return,
        emit_assign,
        emit_test
    };
    
    string_queue = list_create();
    
    printf("    .file \"%s\"\n", module->filename);
    printf("    .text\n");
    printf("\n");
    
    the_module = module;
    
    for (i = 0; i < tree_num_children(module); i++)
    {
        FUNCTION *func = tree_get_child(module, i);
        if (tree_get_child(func, 0) && (CAST_TO_DECLARATION(func)->use_count > 0 || CAST_TO_DECLARATION(func)->flags & DECL_PUBLIC))
            emit_function(func, &functions, func);
        label_offset = label_offset + label_max + 1;
    }
    
    printf("    .section .rodata\n");
    for (i = 0; i < string_queue->size; i++)
    {
        printf(".LC%d:\n    .string \"%s\"\n", i, (char *) string_queue->items[i]);
    }
    
    list_destroy(string_queue);
    
    return 1;
}
