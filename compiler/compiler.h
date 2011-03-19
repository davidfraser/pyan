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

typedef struct STATEMENT
{
    NODE node;
} STATEMENT;

typedef struct BLOCK
{
    NODE node;
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
    NODE node;
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
    NODE node;
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
    NODE node;
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
extern STATEMENT *make_block(HASH *table, STATEMENT *stmt);
extern STATEMENT *make_if(EXPRESSION *c, STATEMENT *s1, STATEMENT *s2);
extern STATEMENT *make_while(EXPRESSION *c, STATEMENT *s1);
extern STATEMENT *make_for(STATEMENT *init, EXPRESSION *c, STATEMENT *step, STATEMENT *body);
extern STATEMENT *make_assignment(EXPRESSION *n, EXPRESSION *v);
extern STATEMENT *make_return(EXPRESSION *c);
extern STATEMENT *make_continue();
extern STATEMENT *make_break();
extern STATEMENT *make_pass(void);
extern STATEMENT *make_join(void);
extern STATEMENT *make_enter(void);
extern STATEMENT *make_exit(void);
extern STATEMENT *make_restart(void);
extern STATEMENT *make_test(EXPRESSION *c);
extern STATEMENT *make_statements(STATEMENT *s1, STATEMENT *s2);
extern FUNCTION *make_function(TYPE *type, char *name, DECLARATION *args);
extern DECLARATION *make_declaration(TYPE *type, char *name);
extern EXPRESSION *make_binary_expression(NODE_TYPE type, EXPRESSION *a, EXPRESSION *b);
extern EXPRESSION *make_unary_expression(NODE_TYPE type, EXPRESSION *a);
extern EXPRESSION *make_call(EXPRESSION *var, EXPRESSION *args);
extern EXPRESSION *make_closure(MODULE *module, TYPE *type, DECLARATION *args, BLOCK *body);
extern EXPRESSION *make_integer(char *val);
extern EXPRESSION *make_integer_direct(int val);
extern EXPRESSION *make_string(char *str);
extern EXPRESSION *make_variable(char *name);
extern TYPE *make_primitive_type(NODE_TYPE type);
extern TYPE *make_map_type(TYPE *t1, TYPE *t2);
extern TYPE *make_tuple_type(TYPE *t1, TYPE *t2);
extern EXPRESSION *make_tuple(EXPRESSION *expr1, EXPRESSION *expr2);
extern EXPRESSION *make_empty_tuple(void);
extern GRAPH *make_graph(FUNCTION *func);
extern EXPRESSION *get_input_tuple(FUNCTION *func);
extern EXPRESSION *make_new_temp(MODULE *module, FUNCTION *func, TYPE *type);

extern int is_unary_op(EXPRESSION *expr);
extern int is_comparison_op(EXPRESSION *expr);
extern int is_binary_op(EXPRESSION *expr);
extern int is_commutable_op(EXPRESSION *expr);
extern VALUE evaluate_binary_op(STATE *state, EXPRESSION *expr);
extern VALUE evaluate_unary_op(STATE *state, EXPRESSION *expr);
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
