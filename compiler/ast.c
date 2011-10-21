#include "compiler.h"

#include <stdio.h>
#include <string.h>


static void *create_ast_node(NODE_TYPE type, int source_line)
{
    AST *n = (AST *) tree_create_node(type);
    n->source_line = source_line;
    return (void *) n;
}


STATEMENT *make_block(HASH *table, STATEMENT *stmt, int source_line)
{
    if (tree_is_type(stmt, STMT_BLOCK))
        return stmt;

    if (source_line == 0 && stmt)
        source_line = CAST_TO_AST(stmt)->source_line;
    
    BLOCK *block = CAST_TO_BLOCK(create_ast_node(STMT_BLOCK, source_line));
    block->table = table ? table : create_hash(10, key_type_copyable);
    if (tree_is_type(stmt, STMT_SEQUENCE))
    {
        int i;
        
        for (i = 0; i < tree_num_children(stmt); i++)
            tree_add_child(block, tree_get_child(stmt, i));
    }
    else
        tree_add_child(block, stmt);
    return CAST_TO_STATEMENT(block);
}


STATEMENT *make_if(EXPRESSION *c, STATEMENT *s1, STATEMENT *s2, int source_line)
{
    if (source_line == 0 && c)
        source_line = CAST_TO_AST(c)->source_line;
    NODE *node = create_ast_node(STMT_IF, source_line);
    tree_add_child(node, c);
    tree_add_child(node, make_block(NULL, s1, 0));
    tree_add_child(node, make_block(NULL, s2, 0));
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_while(EXPRESSION *c, STATEMENT *s1, int source_line)
{
    if (source_line == 0 && c)
        source_line = CAST_TO_AST(c)->source_line;
    NODE *node = create_ast_node(STMT_WHILE, source_line);
    tree_add_child(node, c);
    tree_add_child(node, make_block(NULL, s1, 0));
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_for(STATEMENT *init, EXPRESSION *c, STATEMENT *step, STATEMENT *body, int source_line)
{
    NODE *node = create_ast_node(STMT_FOR, source_line);
    tree_add_child(node, init);
    tree_add_child(node, c);
    tree_add_child(node, step);
    tree_add_child(node, body);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_assignment(EXPRESSION *n, EXPRESSION *v, int source_line)
{
    NODE *node = create_ast_node(STMT_ASSIGN, source_line);
    tree_add_child(node, n);
    tree_add_child(node, v);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_return(EXPRESSION *c, int source_line)
{
    NODE *node = create_ast_node(STMT_RETURN, source_line);
    tree_add_child(node, c);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_continue(int source_line)
{
    NODE *node = create_ast_node(STMT_CONTINUE, source_line);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_break(int source_line)
{
    NODE *node = create_ast_node(STMT_BREAK, source_line);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_pass(int source_line)
{
    NODE *node = create_ast_node(STMT_PASS, source_line);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_join(int source_line)
{
    NODE *node = create_ast_node(STMT_JOIN, source_line);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_enter(int source_line)
{
    NODE *node = create_ast_node(STMT_ENTER, source_line);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_exit(int source_line)
{
    NODE *node = create_ast_node(STMT_EXIT, source_line);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_restart(int source_line)
{
    NODE *node = create_ast_node(STMT_RESTART, source_line);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_test(EXPRESSION *c, int source_line)
{
    if (source_line == 0 && c)
        source_line = CAST_TO_AST(c)->source_line;
    NODE *node = create_ast_node(STMT_TEST, source_line);
    tree_add_child(node, c);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_statements(STATEMENT *s1, STATEMENT *s2, int source_line)
{
    NODE *node;
    if (tree_is_type(s1, STMT_SEQUENCE))
        node = CAST_TO_NODE(s1);
    else
    {
        node = create_ast_node(STMT_SEQUENCE, source_line);
        tree_add_child(node, s1);
    }
    tree_add_child(node, s2);
    return CAST_TO_STATEMENT(node);
}


FUNCTION *make_function(TYPE *type, char *name, DECLARATION *args, int source_line)
{
    FUNCTION *func = create_ast_node(DEF_FUNCTION, source_line);
    DECLARATION *decl = CAST_TO_DECLARATION(func);
    decl->name = name;
    decl->type = make_map_type(args->type, type, source_line);
    decl->flags |= DECL_STATIC;
    tree_add_child(func, NULL);
    tree_add_child(func, args);
    return func;
}


DECLARATION *make_declaration(TYPE *type, char *name, int source_line)
{
    DECLARATION *decl = create_ast_node(DEF_VARIABLE, source_line);
    decl->type = type;
    decl->name = name;
    return decl;
}


EXPRESSION *make_binary_expression(NODE_TYPE type, EXPRESSION *a, EXPRESSION *b, int source_line)
{
    EXPRESSION *expr = create_ast_node(type, source_line);
    tree_add_child(expr, a);
    tree_add_child(expr, b);
    expr->type = a->type;
    return expr;
}


EXPRESSION *make_unary_expression(NODE_TYPE type, EXPRESSION *a, int source_line)
{
    EXPRESSION *expr = create_ast_node(type, source_line);
    tree_add_child(expr, a);
    expr->type = a->type;
    return expr;
}


EXPRESSION *make_call(EXPRESSION *var, EXPRESSION *args, int source_line)
{
    EXPRESSION *expr = create_ast_node(EXPR_CALL, source_line);
    tree_add_child(expr, var);
    tree_add_child(expr, args);
    
    expr->type = tree_get_child(var->type, 1);
    return expr;
}


EXPRESSION *make_closure(MODULE *mod, TYPE *type, DECLARATION *args, BLOCK *body, int source_line)
{
    static int next_id = 0;
    char name[100];
    
    sprintf(name, "closure%d", next_id++);
    char *str = add_string(mod, name, strlen(name));
    
    FUNCTION *func = make_function(type, str, args, source_line);
    tree_get_child(func, 0) = body;
    EXPRESSION *expr = create_ast_node(EXPR_CLOSURE, source_line);
    tree_add_child(expr, func);
    expr->type = make_map_type(args->type, type, source_line);
    
    /* Add new function to module. */
    tree_add_child(mod, func);
    add_to_hash(mod->table, str, strlen(str), func);
    return expr;
}


EXPRESSION *make_integer_direct(int val, int source_line)
{
    INTEGER *expr = create_ast_node(EXPR_INTEGER, source_line);
    expr->value = val;
    expr->super.type = make_primitive_type(TYPE_INT, source_line);
    return CAST_TO_EXPRESSION(expr);
}


EXPRESSION *make_integer(char *val, int source_line)
{
    int x = atoi(val);
    return make_integer_direct(x, source_line);
}


EXPRESSION *make_string(char *str, int source_line)
{
    STRING *expr = create_ast_node(EXPR_STRING, source_line);
    expr->value = str;
    return CAST_TO_EXPRESSION(expr);
}


EXPRESSION *make_variable(char *name, int source_line)
{
    VARIABLE *expr = create_ast_node(EXPR_VARIABLE, source_line);
    expr->name = name;
    return CAST_TO_EXPRESSION(expr);
}


TYPE *make_primitive_type(NODE_TYPE type, int source_line)
{
    TYPE *t = create_ast_node(type, source_line);
    return t;
}


TYPE *make_map_type(TYPE *t1, TYPE *t2, int source_line)
{
    TYPE *type = create_ast_node(TYPE_MAP, source_line);
    tree_add_child(type, t1);
    tree_add_child(type, t2);
    return type;
}


TYPE *make_tuple_type(TYPE *t1, TYPE *t2, int source_line)
{
    NODE *node;
    if (tree_is_type(t1, TYPE_TUPLE))
        node = CAST_TO_NODE(t1);
    else
    {
        node = create_ast_node(TYPE_TUPLE, source_line);
        tree_add_child(node, t1);
    }
    tree_add_child(node, t2);
    return CAST_TO_TYPE(node);
}


EXPRESSION *make_tuple(EXPRESSION *expr1, EXPRESSION *expr2, int source_line)
{
    EXPRESSION *node;
    if (tree_is_type(expr1, EXPR_TUPLE))
        node = expr1;
    else
    {
        node = create_ast_node(EXPR_TUPLE, source_line);
        tree_add_child(node, expr1);
        node->type = create_ast_node(TYPE_TUPLE, source_line);
        tree_add_child(node->type, expr1->type);
    }
    tree_add_child(node, expr2);
    tree_add_child(node->type, expr2->type);
    return node;
}


EXPRESSION *make_empty_tuple(int source_line)
{
    NODE *node;
    node = create_ast_node(EXPR_TUPLE, source_line);
    return CAST_TO_EXPRESSION(node);
}


EXPRESSION *get_input_tuple(FUNCTION *func)
{
    int source_line = CAST_TO_AST(func)->source_line;
    EXPRESSION *tuple =  create_ast_node(EXPR_TUPLE, source_line);
    DECLARATION *args = tree_get_child(func, 1);
    
    if (!args)
        return make_empty_tuple(source_line);
    
    int i;
    for (i = 0; i < tree_num_children(args); i++)
    {
        DECLARATION *v = tree_get_child(args, i);
        EXPRESSION *arg = make_variable(v->name, CAST_TO_AST(v)->source_line);
        arg->type = v->type;
        ((VARIABLE *) arg)->decl = v;
        tree_add_child(tuple, arg);
    }
    
    if (tree_num_children(tuple) == 1)
        return tree_get_child(tuple, 0);

    return tuple;
}


EXPRESSION *make_new_temp(MODULE *module, FUNCTION *func, TYPE *type, int source_line)
{
    static int next_id = 0;
    char next_name[100];
    
    if (!type)
        error("declaration needs a type!");
    
    sprintf(next_name, "$t%d", next_id++);
    char *str = add_string(module, next_name, strlen(next_name));
    
    DECLARATION *new_temp = make_declaration(type, str, source_line);
    add_to_hash(func->table, str, strlen(str), new_temp);
    VARIABLE *new_var = CAST_TO_VARIABLE(make_variable(str, source_line));
    new_var->decl = new_temp;
    EXPRESSION *new_expr = CAST_TO_EXPRESSION(new_var);
    new_expr->type = type;
    return new_expr;
}
