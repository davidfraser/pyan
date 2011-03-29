/*
 * lex.c - Lexical analyser for the theorem prover.
 *
 * Copyright (C) 2003, Edmund Horner.
 */
 
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "lex.h"

typedef struct KEYWORD
{
    char *word;
    int id;
} KEYWORD;

KEYWORD keywords[] =
{
    { "==", EQ_KEYWORD },
    { "!=", NEQ_KEYWORD },
    { "<=", LEQ_KEYWORD },
    { ">=", GEQ_KEYWORD },
    { "&&", AND_KEYWORD },
    { "||", OR_KEYWORD },
    { "->", MAP_KEYWORD },
    { "lambda", LAMBDA_KEYWORD },
    { "void", VOID_KEYWORD },
    { "int", INT_KEYWORD },
    { "for", FOR_KEYWORD },
    { "if", IF_KEYWORD },
    { "else", ELSE_KEYWORD },
    { "while", WHILE_KEYWORD },
    { "return", RETURN_KEYWORD },
    { "continue", CONTINUE_KEYWORD },
    { "break", BREAK_KEYWORD },
    { "public", PUBLIC_KEYWORD },
    { NULL }
};

static HASH *keyword_map;

static void populate_keywords()
{
    KEYWORD *k;
    
    keyword_map = create_hash(20, key_type_indirect);

    for (k = keywords; k->word; k++)
    {
        add_to_hash(keyword_map, k->word, strlen(k->word), (void *) k->id);
    }    
}

static int lookup_keyword(char *word)
{
    if (!keyword_map)
        populate_keywords();
    
    int val = (int) get_from_hash(keyword_map, word, strlen(word));
    
    return val;
}


static int isname(int a)
{
    return isalnum(a) || (a == '_');
}

static int issymbol(int a)
{
    return ispunct(a) && (a != '_');
}

static int same_token(int a, int b)
{
    if (isname(a) && isname(b))
        return 1;

    if (isname(a) && isdigit(b))
        return 1;

    if (isdigit(a) && isdigit(b))
        return 1;
    
    if (issymbol(a) && issymbol(b) && a != '(' && a != ')' && b != '(' && b != ')')
        return 1;
    
    if (a == '-' && isdigit(b))
        return 1;

    return 0;
}

int yylex(YYSTYPE *yylval, YYLTYPE *loc, PARSER *parser)
{
    char *q;
    int len;
    int kw_id;
    
    int in_comment = 0;
    
    do
    {
        if (!(*parser->p))
        {
            if (!parser->input(parser->buffer, parser->file))
                return 0;
            
            parser->p = parser->buffer;
        }
        
        if (!in_comment)
        {
            if (*parser->p == '/' && *(parser->p+1) == '*')
            {
                parser->p += 2;
                in_comment = 1;
                
                while (*parser->p)
                {
                    if (*parser->p == '*' && *(parser->p+1) == '/')
                    {
                        parser->p += 2;
                        in_comment = 0;
                        break;
                    }
                    
                    parser->p++;
                }
            }
            else if (*parser->p == '/' && *(parser->p+1) == '/')
            {
                *parser->p = 0;
            }
            else if (isspace(*parser->p))
            {
                if (*parser->p == '\n')
                    loc->first_line++;
                parser->p++;
            }
            else
            {
                break;
            }
        }
        else
        {
            while (*parser->p)
            {
                if (*parser->p == '*' && *(parser->p+1) == '/')
                {
                    parser->p += 2;
                    in_comment = 0;
                    break;
                }
                
                parser->p++;
            }
            loc->first_line++;
        }
    }
    while (1);
    
    q = parser->p;
    if (*q == '\"')
    {
        parser->p++;
        while (*parser->p && *parser->p != '\"')
        {
            parser->p++;
        }
        
        parser->p++;
    }
    else
    {
        parser->p++;
        while (*parser->p && same_token(*(parser->p-1), *parser->p))
        {
            parser->p++;
        }
    }
    
    if (q > parser->p)
        q = parser->buffer;
    len = (*q != '\"') ? parser->p - q : parser->p - q - 2;
    
    char *str = add_string(parser->module, (*q != '\"') ? q : q + 1, len);
    
    yylval->name = str;
    
    loc->first_column = q - parser->buffer + 1;
    
    //printf("[%s", yylval->name);
    
    if (*q == '\"')
        return STRING_CONSTANT;
    
    if (issymbol(yylval->name[0]) && yylval->name[1] == 0)
    {
        //printf(" -> %d]\n", yylval->name[0]);
        return yylval->name[0];
    }

    kw_id = lookup_keyword(yylval->name);
    if (kw_id)
    {
        //printf(" -> %d]\n", k->id);
        return kw_id;
    }
    
    if (isdigit(yylval->name[0]) || yylval->name[0] == '-')
    {
        //printf(" -> %d]\n", yylval->ch);
        return INT_CONSTANT;
    }
    
    //printf(" -> NAME]\n");
    return NAME;
}

void yyerror(YYLTYPE *loc, PARSER *parser, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s:%d:%d: ", /*parser->module->filename*/ "TODO", loc->first_line, loc->first_column);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}
