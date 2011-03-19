/*
 * grammar.y - Grammar for the compiler.
 *
 * Copyright (C) 2003, Edmund Horner.
 */

%{
#include <stdio.h>
#include <string.h>

#include "lex.h"
#include "compiler.h"

static void add_body(FUNCTION *func, BLOCK *body)
{
    tree_get_child(func, 0) = body;
}

static void add_declaration(HASH *table, DECLARATION *decl)
{
    add_to_hash(table, decl->name, strlen(decl->name), decl);
}

static DECLARATION *add_arg(DECLARATION *list, DECLARATION *arg)
{
    if (list == NULL)
        list = CAST_TO_DECLARATION(tree_create_node(DEF_ARGS));
    
    tree_add_child(list, arg);
    
    return list;
}

static void push_scope(PARSER *parser, HASH *scope)
{
    add_to_hash(scope, "$parent", strlen("$parent"), parser->scope);
    parser->scope = scope;
}

static void pop_scope(PARSER *parser)
{
    HASH_ENTRY *he = find_in_hash(parser->scope, "$parent", strlen("$parent"));
    parser->scope = he ? he->data : NULL;
}

static DECLARATION *find_declaration(PARSER *parser, char *name)
{
    HASH *scope = parser->scope;
    while (scope)
    {
        HASH_ENTRY *he;
        he = find_in_hash(scope, name, strlen(name));
        if (he)
            return he->data;
        
        he = find_in_hash(scope, "$parent", strlen("$parent"));
        scope = he ? he->data : NULL;
    }
    
    return NULL;
}

static HASH *make_scope(DECLARATION *args)
{
    HASH *scope = create_hash(10, key_type_copyable);
    
    if (args != NULL)
    {
        int i;
        for (i = 0; i < tree_num_children(args); i++)
        {
            DECLARATION *decl = tree_get_child(args, i);
            add_to_hash(scope, decl->name, strlen(decl->name), decl);
        }
    }
    
    return scope;
}

static int resolve_reference(PARSER *parser, VARIABLE *var, YYLTYPE *loc)
{
    DECLARATION *decl = find_declaration(parser, var->name);
    if (!decl)
    {
        yyerror(loc, parser, "Undeclared variable %s", var->name);
        return 0;
    }
    
    decl->use_count++;
    var->decl = decl;
    var->super.type = decl->type;
    
    return 1;
}

%}

%pure-parser

%parse-param {PARSER *parser}
%lex-param {PARSER *parser}

%start program

%union {
    int ch;
    char *name;
    float *fl;
    EXPRESSION *expr;
    DECLARATION *decl;
    STATEMENT *statement;
    TYPE *type;
}

%token <ch> LEX_ERROR

%right <ch> '('
%left <ch> ')'
%right <ch> '{'
%left <ch> '}'
%right <ch> '['
%left <ch> ']'
%token <ch> ';'
%token <ch> '='
%token <ch> IF_KEYWORD ELSE_KEYWORD FOR_KEYWORD WHILE_KEYWORD RETURN_KEYWORD BREAK_KEYWORD CONTINUE_KEYWORD
%token <ch> '<' '>' '+' '-' '*' '/' EQ_KEYWORD NEQ_KEYWORD LEQ_KEYWORD GEQ_KEYWORD AND_KEYWORD OR_KEYWORD
%token <name> NAME INT_CONSTANT FLOAT_CONSTANT STRING_CONSTANT MAP_KEYWORD LAMBDA_KEYWORD
%token <ch> VOID_KEYWORD INT_KEYWORD FLOAT_KEYWORD PUBLIC_KEYWORD

%type <expr> expression expr_list integer float string tuple_content
%type <expr> disjunction conjunction
%type <expr> comparison sum difference product ratio negation atom call closure
%type <expr> name /*name_list*/

%type <decl> def var_def function program
%type <decl> arg_def arg_list

%type <type> type primitive_type map_type tuple_type type_list

%type <statement> assignment_stmt var_def_and_assign
%type <statement> for_stmt return_stmt statements statement block block_inner matched unmatched

%%

program:
      /* empty */ { $$ = NULL; }
    | program def
        {
            DECLARATION *decl = CAST_TO_DECLARATION($2);
            tree_add_child(parser->module, decl);
            add_to_hash(parser->module->table, decl->name, strlen(decl->name), decl);
        }
    ;
    
def:
      var_def
    | function ';'
    | function
        { DECLARATION *f = CAST_TO_DECLARATION($1); add_to_hash(parser->module->table, f->name, strlen(f->name), f); }
        '{' block '}' { add_body(CAST_TO_FUNCTION($1), CAST_TO_BLOCK($4)); $$ = $1; }
    ;
    
function:
      type NAME '(' arg_list ')' 
        { $$ = CAST_TO_DECLARATION(make_function($1, $2, $4)); }
    | PUBLIC_KEYWORD type NAME '(' arg_list ')'
        { $$ = CAST_TO_DECLARATION(make_function($2, $3, $5)); $$->flags |= DECL_PUBLIC; }
    ;
    
var_def:
      type NAME ';'
        { $$ = make_declaration($1, $2);  add_declaration(parser->scope, $$); }
    ;

var_def_and_assign:
      type NAME '=' expression ';'
        {
            DECLARATION *decl = make_declaration($1, $2);
            add_declaration(parser->scope, decl);
            EXPRESSION *var = make_variable($2);
            if (!resolve_reference(parser, (VARIABLE *) var, &@1))
                YYERROR; 
            $$ = make_assignment(var, $4);
        }
    ;

arg_list:
      /* empty */ { $$ = CAST_TO_DECLARATION(tree_create_node(DEF_ARGS)); }
    | arg_def { $$ = add_arg(NULL, $1); parser->args = CAST_TO_DECLARATION($$); }
    | arg_list ',' arg_def { $$ = add_arg($1, $3); }
    ;
    
arg_def:
      type NAME
        { $$ = make_declaration($1, $2); $$->flags |= DECL_ARGUMENT; }
    ;

type:
      primitive_type
    | map_type
    | tuple_type
    ;

map_type:
    '(' type  MAP_KEYWORD type ')' { $$ = make_map_type($2, $4); }
    ;

primitive_type:
      VOID_KEYWORD { $$ = make_primitive_type(TYPE_VOID); }
    | INT_KEYWORD { $$ = make_primitive_type(TYPE_INT); }
    | FLOAT_KEYWORD { $$ = make_primitive_type(TYPE_FLOAT); }
/*    | name
        {
            $$ = NULL;
        } */
    ;

tuple_type:
      '(' type_list ')' { $$ = $2; }
    ;

type_list:
      type
    | type_list ',' type { $$ = make_tuple_type($1, $3); }
    ;

/*name_list:
      name { $$ = NULL; }
    | name_list ',' name
    ;*/

block:
        { HASH *table = make_scope(parser->args); parser->args = NULL; push_scope(parser, table); }
      block_inner
        { pop_scope(parser); $$ = $2; }
    ;

block_inner:
      statements { $$ = make_block(parser->scope, $1); }
    | /* empty */ { $$ = NULL; }
    ;

statements:
      statement
    | statements statement
        { $$ = make_statements($1, $2);}
    ;

statement:
      matched
    | unmatched
    ;

matched:
      assignment_stmt
    | var_def { $$ = NULL; }
    | var_def_and_assign
    | IF_KEYWORD '(' expression ')' matched ELSE_KEYWORD matched
        { $$ = make_if($3, $5, $7); }
    | for_stmt
    | WHILE_KEYWORD '(' expression ')' matched
        { $$ = make_while($3, $5); }
    | CONTINUE_KEYWORD ';' { $$ = make_continue(); }
    | BREAK_KEYWORD ';' { $$ = make_break(); }
    | return_stmt
    | '{' block '}' { $$ = $2; }
    ;

unmatched:
      IF_KEYWORD '(' expression ')' statement
        { $$ = make_if($3, $5, NULL); }
    | IF_KEYWORD '(' expression ')' matched ELSE_KEYWORD unmatched
        { $$ = make_if($3, $5, $7); }
    | WHILE_KEYWORD '(' expression ')' IF_KEYWORD '(' expression ')' statement
        { $$ = make_while($3, make_if($7, $9, NULL)); }
    ;

assignment_stmt:
      call ';' { $$ = make_assignment(NULL, $1); }
    | expression '=' expression ';'
        { $$ = make_assignment($1, $3); }
    ;
    
for_stmt:
      FOR_KEYWORD '(' statement ';' expression ';'
          statement ')' '{' block '}'
        { $$ = make_for($3, $5, $7, $10); }
    ;

return_stmt:
    RETURN_KEYWORD expression ';'
        { $$ = make_return($2); }
    ;
    
expression:
      disjunction
    ;

disjunction:
      conjunction
    | disjunction OR_KEYWORD conjunction
        { $$ = make_binary_expression(EXPR_OR, $1, $3); }
    ;
    
conjunction:
      comparison
    | conjunction AND_KEYWORD comparison
        { $$ = make_binary_expression(EXPR_AND, $1, $3); }
    ;
    
comparison:
      sum
    | sum EQ_KEYWORD sum
        { $$ = make_binary_expression(EXPR_EQ, $1, $3); }
    | sum NEQ_KEYWORD sum
        { $$ = make_binary_expression(EXPR_NEQ, $1, $3); }
    | sum '<' sum
        { $$ = make_binary_expression(EXPR_LT, $1, $3); }
    | sum '>' sum
        { $$ = make_binary_expression(EXPR_GT, $1, $3); }
    | sum LEQ_KEYWORD sum
        { $$ = make_binary_expression(EXPR_LEQ, $1, $3); }
    | sum GEQ_KEYWORD sum
        { $$ = make_binary_expression(EXPR_EQ, $1, $3); }
    ;
    
sum:
      difference
    | sum '+' difference
        { $$ = make_binary_expression(EXPR_SUM, $1, $3); }
    ;
    
difference:
      product
    | difference '-' product
        { $$ = make_binary_expression(EXPR_DIFFERENCE, $1, $3); }
    ;
     
product:
      ratio
    | product '*' ratio
        { $$ = make_binary_expression(EXPR_PRODUCT, $1, $3); }
    ;
    
ratio:
      negation
    | ratio '/' negation
        { $$ = make_binary_expression(EXPR_RATIO, $1, $3); }
    ;

negation:
      atom
    | '-' atom
        { $$ = make_unary_expression(EXPR_NEGATION, $2); }
    ;

atom:
      name
    | call
    | closure
    | integer
    | float
    | string
    | '(' tuple_content ')' { $$ = $2; }
    ;

tuple_content:
      /* empty*/ { $$ = make_empty_tuple(); }
    | expr_list
    ;

expr_list:
      expression
    | tuple_content ',' expression { $$ = make_tuple($1, $3); }
    ;

call: name '(' tuple_content ')'
        { $$ = make_call($1, $3); }
    ;

closure:
    type LAMBDA_KEYWORD '(' arg_list ')' '{' block '}'
        { $$ = make_closure(parser->module, $1, $4, CAST_TO_BLOCK($7)); }
    ;

name:
      NAME { $$ = make_variable($1); if (!resolve_reference(parser, CAST_TO_VARIABLE($$), &@1)) YYERROR; }
    ;
    
integer:
      INT_CONSTANT { $$ = make_integer($1); }
    ;
    
float:
      FLOAT_CONSTANT { $$ = NULL; }
    ;
        
string:
      STRING_CONSTANT { $$ = make_string($1); }
    ;
    
%%
