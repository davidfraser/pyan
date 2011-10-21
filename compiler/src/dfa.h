struct DFA;

#ifndef DFA_H
#define DFA_H

#include "list.h"
#include "hash.h"
#include "compiler.h"


typedef struct DFA_FUNCTIONS
{
    void *(* create_start_set)(void *data, int type);
    void *(* create_default_set)(void *data, int type);
    void (* destroy_set)(void *set);
    int (* analyse)(NODE *vertex, LIST *input, LIST *output, void *data);
    int (* verify)(NODE *vertex, LIST *input, LIST *output, void *data);
} DFA_FUNCTIONS;

typedef enum
{
    DFA_FORWARD = 0x01,
    DFA_BACKWARD = 0x02,
    DFA_ADD_JOINS = 0x04
} DFA_FLAGS;

typedef struct DFA
{
    struct FUNCTION *function;
    struct GRAPH *graph;
    DFA_FUNCTIONS *functions;
    void *data;
    DFA_FLAGS flags;
    
    HASH *inputs;
    HASH *outputs;
} DFA;

extern DFA *create_dfa(struct FUNCTION *func, DFA_FUNCTIONS *functions, void *data, DFA_FLAGS flags);
extern void destroy_dfa(DFA *dfa);
extern int run_dfa(DFA *dfa);


#endif
