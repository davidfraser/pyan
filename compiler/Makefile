#
# Makefile for the compiler
#
# Copyright (C) 2003, Edmund Horner.
#

CFLAGS = -g -Wall -DGC_DEBUG -rdynamic

YACC = bison -d -r all

OBJ=$(filter-out grammar.tab.o, $(patsubst %.c, %.o, $(wildcard *.c))) grammar.tab.o
HEADERS=grammar.tab.h lex.h compiler.h types.h

all: grammar.tab.h ecc

clean:
	rm -f $(OBJ) *.tab.c *.tab.h c c.exe *.output

test: ecc
	for x in `ls examples/*.e` ; do echo " *" $$x ; ./ecc $$x > /dev/null ; done

ecc: $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) -lgc
    
grammar.tab.c: grammar.y
	$(YACC) grammar.y

grammar.tab.h: grammar.y
	$(YACC) grammar.y

%.o: %.c $(HEADERS)
	$(CC) -c $< $(CFLAGS)
