#include "compiler.h"

#include <stdio.h>
#include <string.h>


char *add_string(MODULE *module, char *str, size_t len)
{
    HASH_ENTRY *he = find_in_hash(module->strings, str, len);
    if (he)
        return he->data;
    
    str = strndup(str, len);
    add_to_hash(module->strings, str, len, str);
    return str;
}


STATEMENT *make_block(HASH *table, STATEMENT *stmt)
{
    if (tree_is_type(stmt, STMT_BLOCK))
        return stmt;
    
    BLOCK *block = CAST_TO_BLOCK(tree_create_node(STMT_BLOCK));
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


STATEMENT *make_if(EXPRESSION *c, STATEMENT *s1, STATEMENT *s2)
{
    NODE *node = tree_create_node(STMT_IF);
    tree_add_child(node, c);
    tree_add_child(node, make_block(NULL, s1));
    tree_add_child(node, make_block(NULL, s2));
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_while(EXPRESSION *c, STATEMENT *s1)
{
    NODE *node = tree_create_node(STMT_WHILE);
    tree_add_child(node, c);
    tree_add_child(node, make_block(NULL, s1));
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_for(STATEMENT *init, EXPRESSION *c, STATEMENT *step, STATEMENT *body)
{
    NODE *node = tree_create_node(STMT_FOR);
    tree_add_child(node, init);
    tree_add_child(node, c);
    tree_add_child(node, step);
    tree_add_child(node, body);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_assignment(EXPRESSION *n, EXPRESSION *v)
{
    NODE *node = tree_create_node(STMT_ASSIGN);
    tree_add_child(node, n);
    tree_add_child(node, v);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_return(EXPRESSION *c)
{
    NODE *node = tree_create_node(STMT_RETURN);
    tree_add_child(node, c);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_continue()
{
    NODE *node = tree_create_node(STMT_CONTINUE);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_break()
{
    NODE *node = tree_create_node(STMT_BREAK);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_pass(void)
{
    NODE *node = tree_create_node(STMT_PASS);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_join(void)
{
    NODE *node = tree_create_node(STMT_JOIN);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_enter(void)
{
    NODE *node = tree_create_node(STMT_ENTER);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_exit(void)
{
    NODE *node = tree_create_node(STMT_EXIT);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_restart(void)
{
    NODE *node = tree_create_node(STMT_RESTART);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_test(EXPRESSION *c)
{
    NODE *node = tree_create_node(STMT_TEST);
    tree_add_child(node, c);
    return CAST_TO_STATEMENT(node);
}


STATEMENT *make_statements(STATEMENT *s1, STATEMENT *s2)
{
    NODE *node;
    if (tree_is_type(s1, STMT_SEQUENCE))
        node = CAST_TO_NODE(s1);
    else
    {
        node = tree_create_node(STMT_SEQUENCE);
        tree_add_child(node, s1);
    }
    tree_add_child(node, s2);
    return CAST_TO_STATEMENT(node);
}


FUNCTION *make_function(TYPE *type, char *name, DECLARATION *args)
{
    FUNCTION *func = CAST_TO_FUNCTION(tree_create_node(DEF_FUNCTION));
    DECLARATION *decl = CAST_TO_DECLARATION(func);
    decl->name = name;
    decl->type = make_map_type(args->type, type);
    decl->flags |= DECL_STATIC;
    tree_add_child(func, NULL);
    tree_add_child(func, args);
    return func;
}


DECLARATION *make_declaration(TYPE *type, char *name)
{
    DECLARATION *decl = CAST_TO_DECLARATION(tree_create_node(DEF_VARIABLE));
    decl->type = type;
    decl->name = name;
    return decl;
}


EXPRESSION *make_binary_expression(NODE_TYPE type, EXPRESSION *a, EXPRESSION *b)
{
    EXPRESSION *expr = CAST_TO_EXPRESSION(tree_create_node(type));
    tree_add_child(expr, a);
    tree_add_child(expr, b);
    expr->type = a->type;
    return expr;
}


EXPRESSION *make_unary_expression(NODE_TYPE type, EXPRESSION *a)
{
    EXPRESSION *expr = CAST_TO_EXPRESSION(tree_create_node(type));
    tree_add_child(expr, a);
    expr->type = a->type;
    return expr;
}


EXPRESSION *make_call(EXPRESSION *var, EXPRESSION *args)
{
    EXPRESSION *expr = CAST_TO_EXPRESSION(tree_create_node(EXPR_CALL));
    tree_add_child(expr, var);
    tree_add_child(expr, args);
    
    expr->type = tree_get_child(var->type, 1);
    return expr;
}


EXPRESSION *make_closure(MODULE *mod, TYPE *type, DECLARATION *args, BLOCK *body)
{
    static int next_id = 0;
    char name[100];
    
    sprintf(name, "closure%d", next_id++);
    char *str = add_string(mod, name, strlen(name));
    
    FUNCTION *func = make_function(type, str, args);
    tree_get_child(func, 0) = body;
    EXPRESSION *expr = CAST_TO_EXPRESSION(tree_create_node(EXPR_CLOSURE));
    tree_add_child(expr, func);
    expr->type = make_map_type(args->type, type);
    
    /* Add new function to module. */
    tree_add_child(mod, func);
    add_to_hash(mod->table, str, strlen(str), func);
    return expr;
}


EXPRESSION *make_integer_direct(int val)
{
    INTEGER *expr = CAST_TO_INTEGER(tree_create_node(EXPR_INTEGER));
    expr->value = val;
    expr->super.type = make_primitive_type(TYPE_INT);
    return CAST_TO_EXPRESSION(expr);
}


EXPRESSION *make_integer(char *val)
{
    int x = atoi(val);
    return make_integer_direct(x);
}


EXPRESSION *make_string(char *str)
{
    STRING *expr = CAST_TO_STRING(tree_create_node(EXPR_STRING));
    expr->value = str;
    return CAST_TO_EXPRESSION(expr);
}


EXPRESSION *make_variable(char *name)
{
    VARIABLE *expr = CAST_TO_VARIABLE(tree_create_node(EXPR_VARIABLE));
    expr->name = name;
    return CAST_TO_EXPRESSION(expr);
}


TYPE *make_primitive_type(NODE_TYPE type)
{
    TYPE *t = CAST_TO_TYPE(tree_create_node(type));
    return t;
}


TYPE *make_map_type(TYPE *t1, TYPE *t2)
{
    TYPE *type = CAST_TO_TYPE(tree_create_node(TYPE_MAP));
    tree_add_child(type, t1);
    tree_add_child(type, t2);
    return type;
}


TYPE *make_tuple_type(TYPE *t1, TYPE *t2)
{
    NODE *node;
    if (tree_is_type(t1, TYPE_TUPLE))
        node = CAST_TO_NODE(t1);
    else
    {
        node = tree_create_node(TYPE_TUPLE);
        tree_add_child(node, t1);
    }
    tree_add_child(node, t2);
    return CAST_TO_TYPE(node);
}


EXPRESSION *make_tuple(EXPRESSION *expr1, EXPRESSION *expr2)
{
    EXPRESSION *node;
    if (tree_is_type(expr1, EXPR_TUPLE))
        node = expr1;
    else
    {
        node = CAST_TO_EXPRESSION(tree_create_node(EXPR_TUPLE));
        tree_add_child(node, expr1);
        node->type = CAST_TO_TYPE(tree_create_node(TYPE_TUPLE));
        tree_add_child(node->type, expr1->type);
    }
    tree_add_child(node, expr2);
    tree_add_child(node->type, expr2->type);
    return node;
}


EXPRESSION *make_empty_tuple(void)
{
    NODE *node;
    node = tree_create_node(EXPR_TUPLE);
    return CAST_TO_EXPRESSION(node);
}


GRAPH *make_graph(FUNCTION *func)
{
    GRAPH *graph = CAST_TO_GRAPH(tree_create_node(DEF_GRAPH));
    graph->forward = create_hash(10, key_type_direct);
    graph->backward = create_hash(10, key_type_direct);
    graph->labels = create_hash(10, key_type_direct);
    
    return graph;
}


EXPRESSION *get_input_tuple(FUNCTION *func)
{
    EXPRESSION *tuple =  CAST_TO_EXPRESSION(tree_create_node(EXPR_TUPLE));
    DECLARATION *args = tree_get_child(func, 1);
    
    if (!args)
        return make_empty_tuple();
    
    int i;
    for (i = 0; i < tree_num_children(args); i++)
    {
        DECLARATION *v = tree_get_child(args, i);
        EXPRESSION *arg = make_variable(v->name);
        arg->type = v->type;
        ((VARIABLE *) arg)->decl = v;
        tree_add_child(tuple, arg);
    }
    
    if (tree_num_children(tuple) == 1)
        return tree_get_child(tuple, 0);

    return tuple;
}


EXPRESSION *make_new_temp(MODULE *module, FUNCTION *func, TYPE *type)
{
    static int next_id = 0;
    char next_name[100];
    
    if (!type)
        error("declaration needs a type!");
    
    sprintf(next_name, "$t%d", next_id++);
    char *str = add_string(module, next_name, strlen(next_name));
    
    DECLARATION *new_temp = make_declaration(type, str);
    add_to_hash(func->table, str, strlen(str), new_temp);
    VARIABLE *new_var = CAST_TO_VARIABLE(make_variable(str));
    new_var->decl = new_temp;
    EXPRESSION *new_expr = CAST_TO_EXPRESSION(new_var);
    new_expr->type = type;
    return new_expr;
}


static void print_integer_node(NODE *node)
{
    INTEGER *integer = CAST_TO_INTEGER(node);
    printf("value %d", integer->value);
}


static void print_variable_node(NODE *node)
{
    VARIABLE *var = CAST_TO_VARIABLE(node);
    printf("name '%s'", var->name);
}


static void print_declaration_node(NODE *node)
{
    DECLARATION *decl = CAST_TO_DECLARATION(node);
    printf("name '%s'", decl->name);
}


static int print_table_entry(HASH_ENTRY *he, void *data)
{
    if (strcmp("$parent", he->key))
    {
        printf("    %s:", (char *) he->key);
        tree_print(he->data, 1);
    }
    return 0;
}


static void print_block_node(NODE *node)
{
    BLOCK *block = CAST_TO_BLOCK(node);
    printf("\n");
    walk_hash(block->table, print_table_entry, NULL);
}


static void print_graph_node(NODE *node)
{
    int i;
    GRAPH *graph = CAST_TO_GRAPH(node);
    
    printf("\n");
    
    for (i = 0; i < tree_num_children(node); i++)
    {
        HASH_ENTRY *he;
        
        printf("%d ->", i);
        he = find_in_hash(graph->forward, tree_get_child(node, i), sizeof(void *));
        if (he)
        {
            HASH *subhash = he->data;
            HASH_ITERATOR iter;
            for (hash_iterator(subhash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
            {
                NODE *n = iter.entry->key;
                HASH_ENTRY *he2 = find_in_hash(graph->labels, n, sizeof(void *));
                if (he2)
                    printf(" %d", (int) he2->data);
            }
        }
        printf("\n");
        
        tree_print(tree_get_child(node, i), 2);
    }
}


void register_node_types(void)
{
    #define BASE_TYPE(n, s, p) tree_register_node_type(n, #n, sizeof(s), p);
    #define NODE_TYPE(n, s, p) tree_register_node_type(n, #n, sizeof(s), p);
    #include "types.h"
    #undef BASE_TYPE
    #undef NODE_TYPE

    tree_register_node_printer(EXPR_INTEGER, print_integer_node);
    tree_register_node_printer(EXPR_VARIABLE, print_variable_node);
    tree_register_node_printer(DEF_VARIABLE, print_declaration_node);
    tree_register_node_printer(DEF_FUNCTION, print_declaration_node);
    tree_register_node_printer(STMT_BLOCK, print_block_node);
    tree_register_node_printer(DEF_GRAPH, print_graph_node);
}


#define BASE_TYPE(n, s, p) s *CAST_TO_##s(void *ptr) { if (!tree_check_type(ptr, n)) error("Invalid cast of type %d to %d!", ((NODE *) ptr)->type, n); return (s *) ptr; }
#define NODE_TYPE(n, s, p)
#include "types.h"
#undef BASE_TYPE
#undef NODE_TYPE
