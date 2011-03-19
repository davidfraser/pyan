/*
 * lex.h - Definitions for the lexical analyser.
 *
 * Copyright (C) 2003, Edmund Horner.
 */

#ifndef LEX_H
#define LEX_H

#include "compiler.h"
#include "grammar.tab.h"

#define MAX_NAME_LEN 256

typedef struct PARSER
{
    char *filename;
    FILE *file;
    char *buffer;
    char *p;
    int (* input)(char *dest, FILE *f);
    MODULE *module;
    HASH *scope;
    DECLARATION *args;
} PARSER;

extern int yyparse(PARSER *parser);
extern int yylex(YYSTYPE *yylval, YYLTYPE *loc, PARSER *parser);
extern void yyerror(YYLTYPE *loc, PARSER *parser, char *fmt, ...);

#endif
