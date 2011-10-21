struct FUNCTION;
struct GRAPH;
struct TYPE;

#ifndef COMPILER_H
#define COMPILER_H

#include "tree.h"
#include "hash.h"
#include "dfa.h"

enum NODE_TYPE
{
    NULL_NODE_TYPE = 0,
#define BASE_TYPE(n, s, p) n,
#define NODE_TYPE(n, s, p) n,
#include "types.h"
#undef BASE_TYPE
#undef NODE_TYPE
    NUM_NODE_TYPES
};

typedef struct MODULE
{
    NODE node;
    char *filename;
    HASH *table;
    HASH *strings;
    int max_registers;
} MODULE;


typedef struct AST
{
    NODE node;
    int source_line;
} AST;


typedef struct STATEMENT
{
    AST ast_node;
} STATEMENT;

typedef struct BLOCK
{
    STATEMENT stmt;
    HASH *table;
} BLOCK;

typedef enum
{
    DECL_ARGUMENT = 0x01,
    DECL_PUBLIC = 0x02,
    DECL_STATIC = 0x04,
    DECL_ENCLOSED = 0x08,
    DECL_CONST = 0x010
} DECL_FLAG;

typedef struct DECLARATION
{
    AST ast_node;
    char *name;
    int use_count;
    int stack_position;
    DECL_FLAG flags;
    struct TYPE *type;
    int depth;
    int colour;
} DECLARATION;

typedef struct FUNCTION
{
    DECLARATION decl;
    HASH *table;
    int input_size;
    int stack_size;
    int output_size;
    struct GRAPH *graph;
    struct DFA *liveness;
} FUNCTION;

typedef struct GRAPH
{
    NODE node;
    HASH *forward;
    HASH *backward;
    HASH *labels;
} GRAPH;

typedef struct EXPRESSION
{
    AST ast_node;
    struct TYPE *type;
} EXPRESSION;

typedef struct VARIABLE
{
    struct EXPRESSION super;
    char *name;
    DECLARATION *decl;
} VARIABLE;

typedef struct INTEGER
{
    struct EXPRESSION super;
    int value;
} INTEGER;

typedef struct STRING
{
    struct EXPRESSION super;
    char *value;
} STRING;

typedef struct TYPE
{
    AST ast_node;
    int size;
} TYPE;

typedef int VALUE;

typedef struct STATE
{
    HASH *globals;
    HASH *locals;
    MODULE *program;
    VALUE parameter;
    VALUE return_value;
} STATE;

typedef enum
{
    EDGE_NORMAL = 0x01,
    EDGE_YES = 0x02,
    EDGE_NO = 0x04,
    EDGE_BACK = 0x08,
    EDGE_LOOP = 0x10,
    EDGE_SYMMETRICAL = 0x20
} EDGE_TYPE;


typedef struct DAA_SET
{
    EDGE_TYPE type;
    HASH *set;
    int first_time;
} DAA_SET;


typedef struct EMIT_FUNCTIONS
{
    void (* emit_comment)(NODE *vertex, void *data);
    void (* emit_enter)(NODE *vertex, void *data);
    void (* emit_exit)(NODE *vertex, void *data);
    void (* emit_end)(void *data);
    void (* emit_label)(int label, void *data);
    void (* emit_jump)(int label, void *data);
    void (* emit_return)(NODE *vertex, void *data);
    void (* emit_assign)(NODE *vertex, void *data);
    void (* emit_test)(NODE *vertex, EDGE_TYPE branch_type, int label, void *data);
} EMIT_FUNCTIONS;


extern char *add_string(MODULE *module, char *str, size_t len);
extern GRAPH *make_graph(FUNCTION *func);

extern STATEMENT *make_block(HASH *table, STATEMENT *stmt, int source_line);
extern STATEMENT *make_if(EXPRESSION *c, STATEMENT *s1, STATEMENT *s2, int source_line);
extern STATEMENT *make_while(EXPRESSION *c, STATEMENT *s1, int source_line);
extern STATEMENT *make_for(STATEMENT *init, EXPRESSION *c, STATEMENT *step, STATEMENT *body, int source_line);
extern STATEMENT *make_assignment(EXPRESSION *n, EXPRESSION *v, int source_line);
extern STATEMENT *make_return(EXPRESSION *c, int source_line);
extern STATEMENT *make_continue(int source_line);
extern STATEMENT *make_break(int source_line);
extern STATEMENT *make_pass(int source_line);
extern STATEMENT *make_join(int source_line);
extern STATEMENT *make_enter(int source_line);
extern STATEMENT *make_exit(int source_line);
extern STATEMENT *make_restart(int source_line);
extern STATEMENT *make_test(EXPRESSION *c, int source_line);
extern STATEMENT *make_statements(STATEMENT *s1, STATEMENT *s2, int source_line);
extern FUNCTION *make_function(TYPE *type, char *name, DECLARATION *args, int source_line);
extern DECLARATION *make_declaration(TYPE *type, char *name, int source_line);
extern EXPRESSION *make_binary_expression(NODE_TYPE type, EXPRESSION *a, EXPRESSION *b, int source_line);
extern EXPRESSION *make_unary_expression(NODE_TYPE type, EXPRESSION *a, int source_line);
extern EXPRESSION *make_call(EXPRESSION *var, EXPRESSION *args, int source_line);
extern EXPRESSION *make_closure(MODULE *module, TYPE *type, DECLARATION *args, BLOCK *body, int source_line);
extern EXPRESSION *make_integer(char *val, int source_line);
extern EXPRESSION *make_integer_direct(int val, int source_line);
extern EXPRESSION *make_string(char *str, int source_line);
extern EXPRESSION *make_variable(char *name, int source_line);
extern TYPE *make_primitive_type(NODE_TYPE type, int source_line);
extern TYPE *make_map_type(TYPE *t1, TYPE *t2, int source_line);
extern TYPE *make_tuple_type(TYPE *t1, TYPE *t2, int source_line);
extern EXPRESSION *make_tuple(EXPRESSION *expr1, EXPRESSION *expr2, int source_line);
extern EXPRESSION *make_empty_tuple(int source_line);
extern EXPRESSION *get_input_tuple(FUNCTION *func);
extern EXPRESSION *make_new_temp(MODULE *module, FUNCTION *func, TYPE *type, int source_line);

extern int evaluate_binary_op(EXPRESSION *expr);
extern int is_unary_op(EXPRESSION *expr);
extern int is_comparison_op(EXPRESSION *expr);
extern int is_binary_op(EXPRESSION *expr);
extern int is_commutable_op(EXPRESSION *expr);
extern char *get_op_symbol(EXPRESSION *expr);

extern void add_vertex(GRAPH *graph, NODE *vertex);
extern void add_edge(GRAPH *graph, NODE *from, NODE *to, EDGE_TYPE type);
extern void remove_vertex(GRAPH *graph, NODE *vertex);
extern void remove_edge(GRAPH *graph, NODE *from, NODE *to);
extern void inject_before(GRAPH *graph, NODE *vertex, NODE *before, EDGE_TYPE type);
extern void replace_forward(GRAPH *graph, NODE *old, NODE *vertex, EDGE_TYPE type);
extern void replace_backward(GRAPH *graph, NODE *old, NODE *vertex, EDGE_TYPE type);
extern void cleanup_graph(FUNCTION *func);
extern void print_graph(GRAPH *graph, char *name, void *data);


#include <stdio.h>
#include <execinfo.h>

#define error(...) do { void *buffer[100]; int num; fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); num = backtrace(buffer, 100); backtrace_symbols_fd(buffer, num, fileno(stderr)); exit(1); } while(0)

extern void register_node_types(void);

#define BASE_TYPE(n, s, p) extern s *CAST_TO_##s(void *ptr);
#define NODE_TYPE(n, s, p)
#include "types.h"
#undef BASE_TYPE
#undef NODE_TYPE

#endif
