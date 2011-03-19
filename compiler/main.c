/*
 * main.c - Main file for the compiler.
 *
 * Copyright (C) 2003, Edmund Horner.
 *
 * This program is an experimental compiler; its purpose is to teach
 * me how compilers work.
 *
 * SIGNATURE:     ExperimentalCompilerProgram : Edmund x CompilerDesignHelp
 *                  -> AWorkingCompiler
 * PRECONDITION:  Edmund knows little about compilers, but CompilerDesignHelp
 *                  exists in the form of wise people, good books, and strong
 *                  coffee.
 * POSTCONDITION: A compiler that works, and a nice feeling of achievement.
 *
 * Seriously: the compiler program reads a source file from standard input,
 * and outputs assembly code equivalent to that file.  Optionally, it will
 * also output fascinating information about the parsing, optimisation and
 * code generation stages.
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gc.h>

#include "lex.h"
#include "tree.h"
#include "compiler.h"


struct OPTIONS
{
    char *filename;
    int graphs;
};


static void print_help(void)
{
    printf("Experimental compiler for the e language\n");
    printf("Command line:   c [option...] [file]\n");
    printf("Options:\n");
    printf("   -h   print this help message\n");
    printf("   -g   output some graphs in .dot format\n");
    printf("If file is not specified then standard input is used.\n");
    exit(0);
}


static int parse_command_line(int argc, char *argv[], struct OPTIONS *options)
{
    int i;
    
    for (i = 1; i < argc; i++)
    {
        char *arg = argv[i];
        if (arg[0] == '-')
            switch (arg[1])
            {
                case 'h':
                    print_help();
                    break;
                case 'g':
                    options->graphs = 1;
                    fprintf(stderr, "Graph output enabled\n");
                    break;
            }
        else
        {
            options->filename = arg;
            fprintf(stderr, "Using file: %s\n", options->filename);
        }
    }
    
    return 1;
}


#define MAX_LINE 4096

/*
 * This function is called by the lexical analyser to read another line of
 * input from standard input.  (A token is guaranteed not to take up more than
 * one line, so the lexical analyser only needs to know about one line at a
 * time.)
 */
static int get_line(char *dest, FILE *f)
{
    fgets(dest, MAX_LINE, f);
    
    return !feof(f);
};


static void print_graphs(MODULE *module)
{
    int i;
    
    printf("digraph G {\n");
    for (i = 0; i < tree_num_children(module); i++)
    {
        FUNCTION *func = tree_get_child(module, i);
        if (func->graph == NULL)
            continue;
        print_graph(func->graph, CAST_TO_DECLARATION(func)->name, NULL);
    }
    printf("}\n");
}


typedef int (* FUNCTION_PROCESSOR)(MODULE *module, FUNCTION *func);


static int process_functions(MODULE *module, FUNCTION_PROCESSOR proc)
{
    int changed = 0;
    int i;
    
    for (i = 0; i < tree_num_children(module); i++)
    {
        NODE *node = tree_get_child(module, i);
        if (!tree_is_type(node, DEF_FUNCTION))
            continue;
        
        FUNCTION *func = CAST_TO_FUNCTION(node);
        if (tree_get_child(func, 0))
            changed |= proc(module, func);
    }
    
    return changed;
}


extern int analyse_tail_recursion(MODULE *mod, FUNCTION *func);
extern int analyse_symbols(MODULE *mod, FUNCTION *func);
extern int process_closures(MODULE *mod, FUNCTION *func);
extern int flatten(MODULE *mod, FUNCTION *func);
extern int reduce(MODULE *mod, FUNCTION *func);
extern int definite_assignment_analysis(MODULE *mod, FUNCTION *func);
extern int analyse_inlining(MODULE *mod, FUNCTION *func);
extern int i386ify(MODULE *mod, FUNCTION *func);
extern int register_allocation(MODULE *mod, FUNCTION *func);
extern int analyse_function_size(MODULE *mod, FUNCTION *func);


static int compile(struct OPTIONS *options)
{
    MODULE *module;
    PARSER parser;
    char buffer[MAX_LINE] = "";
    
    if (options->filename)
    {
        parser.filename = options->filename;
        parser.file = fopen(options->filename, "rt");
    }
    else
    {
        parser.filename = "<stdin>";
        parser.file = stdin;
    }
    
    /*
     * 1. Read and parse source code from standard input.
     */
    module = CAST_TO_MODULE(tree_create_node(DEF_MODULE));
    module->filename = parser.filename;
    module->table = create_hash(10, key_type_copyable);
    module->strings = create_hash(10, key_type_copyable);
    module->max_registers = 6;
    
    parser.buffer = parser.p = buffer;  
    parser.input = get_line;
    parser.module = module;
    parser.scope = module->table;
    parser.args = NULL;
    
    if (yyparse(&parser) == 1)
        return 0;
    
    /*
     * 2. Optimise!
     */
    
    //TODO: Think of some possible optimisations!
    // - Constant folding.
    // - Common subexpression elimination.
    // - Dead code removal.
    // - Function inlining.
    
    process_functions(module, analyse_tail_recursion);
    process_functions(module, analyse_symbols);
    process_functions(module, process_closures);
    process_functions(module, flatten);
    process_functions(module, reduce);
    process_functions(module, definite_assignment_analysis);
    process_functions(module, analyse_inlining);
    
    /*
     * 3. Output assembly code.
     */
    process_functions(module, i386ify);
    process_functions(module, register_allocation);
    process_functions(module, analyse_function_size);
    
    if (options->graphs)
        print_graphs(module);
    else
        generate_as(module);
    
    /*
     * 4. Clean up.
     */
     
    //TODO: May not be necessary as things are generally freed on termination.
    
    tree_destroy_node(CAST_TO_NODE(module));
    
    if (parser.file != stdin)
        fclose(parser.file);
    
    return 1;
}


int main(int argc, char *argv[])
{
    struct OPTIONS options = { NULL, 0 };
    
    GC_INIT();
    
    register_node_types();
    
    parse_command_line(argc, argv, &options);
    
    compile(&options);
    
    return 0;
}
