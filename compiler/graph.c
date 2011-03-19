#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "compiler.h"
#include "dfa.h"


void add_vertex(GRAPH *graph, NODE *vertex)
{
    if (!vertex)
    {
        tree_add_child(graph, vertex);
        return;
    }
    
    HASH_ENTRY *he = find_in_hash(graph->labels, vertex, sizeof(void *));
    if (he)
        return;
    
    add_to_hash(graph->labels, vertex, sizeof(void *), (void *) tree_num_children(graph));
    tree_add_child(graph, vertex);
}


static void add_edge1(HASH *hash, NODE *from, NODE *to, EDGE_TYPE type)
{
    HASH_ENTRY *he = find_in_hash(hash, from, sizeof(void *));
    HASH *subhash;
    if (he)
    {
        subhash = he->data;
    }
    else
    {
        subhash = create_hash(10, key_type_direct);
        add_to_hash(hash, from, sizeof(void *), subhash);
    }
    
    add_to_hash(subhash, to, sizeof(void *), (void *) type);
}


void add_edge(GRAPH *graph, NODE *from, NODE *to, EDGE_TYPE type)
{
    add_edge1(graph->forward, from, to, type);
    add_edge1(graph->backward, to, from, type);
}


void remove_vertex(GRAPH *graph, NODE *vertex)
{
    HASH_ENTRY *he = find_in_hash(graph->labels, vertex, sizeof(void *));
    if (!he)
        return;
    
    int label = (int) he->data;
    graph->node.children->items[label] = NULL;
    remove_from_hash(graph->labels, vertex, sizeof(void *));
}


static void remove_edge1(HASH *hash, NODE *from, NODE *to)
{
    HASH_ENTRY *he = find_in_hash(hash, from, sizeof(void *));
    HASH *subhash;
    if (!he)
        return;
    
    subhash = he->data;
    remove_from_hash(subhash, to, sizeof(void *));
    
    if (subhash->num == 0)
        remove_from_hash(hash, from, sizeof(void *));
}


void remove_edge(GRAPH *graph, NODE *from, NODE *to)
{
    remove_edge1(graph->forward, from, to);
    remove_edge1(graph->backward, to, from);
}


void inject_before(GRAPH *graph, NODE *vertex, NODE *before, EDGE_TYPE type)
{
    HASH *subhash = get_from_hash(graph->backward, before, sizeof(void *));
    HASH_ITERATOR iter;
    for (hash_iterator(subhash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        NODE *pred = iter.entry->key;
        EDGE_TYPE type = (EDGE_TYPE) iter.entry->data;
        remove_edge(graph, pred, before);
        add_edge(graph, pred, vertex, type);
    }
    
    add_edge(graph, vertex, before, EDGE_NORMAL | type);
}


void replace_forward(GRAPH *graph, NODE *old, NODE *vertex, EDGE_TYPE type)
{
    HASH *subhash = get_from_hash(graph->forward, old, sizeof(void *));
    HASH_ITERATOR iter;
    for (hash_iterator(subhash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        NODE *succ = iter.entry->key;
        EDGE_TYPE type2 = (EDGE_TYPE) iter.entry->data;
        remove_edge(graph, old, succ);
        add_edge(graph, vertex, succ, type | type2);
    }
}


void replace_backward(GRAPH *graph, NODE *old, NODE *vertex, EDGE_TYPE type)
{
    HASH *subhash = get_from_hash(graph->backward, old, sizeof(void *));
    HASH_ITERATOR iter;
    for (hash_iterator(subhash, &iter); hash_iterator_valid(&iter); hash_iterator_next(&iter))
    {
        NODE *pred = iter.entry->key;
        EDGE_TYPE type2 = (EDGE_TYPE) iter.entry->data;
        remove_edge(graph, pred, old);
        add_edge(graph, pred, vertex, type | type2);
    }
}


void cleanup_graph(FUNCTION *func)
{
    GRAPH *graph = func->graph;
    int i;
    
restart:
    for (i = 2; i < tree_num_children(graph); i++)
    {
        NODE *vertex = tree_get_child(graph, i);
        if (vertex == NULL)
            continue;
        
        if (!find_in_hash(graph->forward, vertex, sizeof(void *))
            || !find_in_hash(graph->backward, vertex, sizeof(void *)))
        {
            //printf("   dead-end: ");
            //tree_print(vertex, 2);
        }
        
        if (tree_is_type(vertex, STMT_PASS))
        {
            HASH *subhash = get_from_hash(graph->forward, vertex, sizeof(void *));
            HASH_ITERATOR iter;
            hash_iterator(subhash, &iter);
            if (hash_iterator_valid(&iter))
            {
                NODE *successor = iter.entry->key;
                EDGE_TYPE type = (EDGE_TYPE) iter.entry->data;
                replace_backward(graph, vertex, successor, type);
                remove_edge(graph, vertex, successor);
                remove_vertex(graph, vertex);
                goto restart;
            }
        }
        else if (tree_is_type(vertex, STMT_JOIN))
        {
            HASH *subhash = get_from_hash(graph->forward, vertex, sizeof(void *));
            if (subhash->num != 1)
                error("Join does not have exactly 1 successor");
            HASH_ITERATOR iter;
            hash_iterator(subhash, &iter);
            if (hash_iterator_valid(&iter))
            {
                NODE *successor = iter.entry->key;
                EDGE_TYPE type = (EDGE_TYPE) iter.entry->data;
                replace_backward(graph, vertex, successor, type);
                remove_edge(graph, vertex, successor);
                remove_vertex(graph, vertex);
                goto restart;
            }
        }
    }
}


char *get_escaped_op_symbol(EXPRESSION *expr)
{
    switch (expr->node.type)
    {
        case EXPR_LEQ:
            return "&lt;=";
        case EXPR_GEQ:
            return "&gt;=";
        case EXPR_LT:
            return "&lt;";
        case EXPR_GT:
            return "&gt;";
        case EXPR_AND:
            return "&amp;&amp;";
        default:
            return get_op_symbol(expr);
    }
}


static char *get_colour(int num)
{
    switch (num)
    {
        case 1:
            return "red";
        case 2:
            return "green";
        case 3:
            return "blue";
        case 4:
            return "brown";
        case 5:
            return "yellow";
        case 6:
            return "orange";
        case 7:
            return "purple";
        default:
            return "gray";
    }
}


void print_expression(EXPRESSION *expr, DAA_SET *set)
{
    if (tree_is_type(expr, EXPR_VARIABLE))
    {
        VARIABLE *var = CAST_TO_VARIABLE(expr);
        int defined = !set || find_in_hash(set->set, var->name, strlen(var->name));
        if (set)
            printf("<font color=\"%s\">", get_colour(var->decl->colour));
        printf("%s", var->name);
        if (set)
            printf("</font>");
    }
    else if (tree_is_type(expr, EXPR_INTEGER))
    {
        INTEGER *integer = CAST_TO_INTEGER(expr);
        printf("%d", integer->value);
    }
    else if (tree_is_type(expr, EXPR_STRING))
    {
        STRING *str = CAST_TO_STRING(expr);
        printf("\"%s\"", str->value);
    }
    else if (tree_is_type(expr, EXPR_TUPLE))
    {
        printf("(");
        if (tree_num_children(expr) >= 1)
        {
            print_expression(tree_get_child(expr, 0), set);
        }
        int i;
        for (i = 1; i < tree_num_children(expr); i++)
        {
            printf(", ");
            print_expression(tree_get_child(expr, i), set);
        }
        printf(")");
    }
    else if (is_unary_op(expr))
    {
        printf("%s", get_escaped_op_symbol(expr));
        print_expression(tree_get_child(expr, 0), set);
    }
    else if (is_binary_op(expr))
    {
        print_expression(tree_get_child(expr, 0), set);
        printf(" %s ", get_escaped_op_symbol(expr));
        print_expression(tree_get_child(expr, 1), set);
    }
    else if (tree_is_type(expr, STMT_ASSIGN))
    {
        printf("assign ");
        print_expression(tree_get_child(expr, 0), set);
        printf(" = ");
        print_expression(tree_get_child(expr, 1), set);
    }
    else if (tree_is_type(expr, STMT_TEST))
    {
        printf("test ");
        print_expression(tree_get_child(expr, 0), set);
    }
    else if (tree_is_type(expr, EXPR_CALL))
    {
        VARIABLE *var = tree_get_child(expr, 0);
        printf("%s(", var->name);
        print_expression(tree_get_child(expr, 1), set);
        printf(")");
    }
    else
        printf("?expr?");
}


static void vertex_printer(NODE *vertex, void *data)
{
    DAA_SET *set = NULL;
    if (data)
    {
        DFA *dfa = data;
        LIST *input = get_from_hash(dfa->inputs, vertex, sizeof(void *));
        set = input->size ? input->items[0] : NULL;
    }
    
    if (tree_is_type(vertex, STMT_ASSIGN))
    {
        EXPRESSION *dest = tree_get_child(vertex, 0);
        print_expression(dest, set);
        printf(" = ");
        EXPRESSION *src = tree_get_child(vertex, 1);
        print_expression(src, set);
    }
    else if (tree_is_type(vertex, STMT_RETURN))
    {
        printf("return ");
        EXPRESSION *expr = tree_get_child(vertex, 0);
        print_expression(expr, set);
    }
    else if (tree_is_type(vertex, STMT_TEST))
    {
        printf("test ");
        EXPRESSION *expr = tree_get_child(vertex, 0);
        print_expression(expr, set);
    }
    else if (tree_is_type(vertex, STMT_PASS))
    {
        printf("pass");
    }
    else if (tree_is_type(vertex, STMT_JOIN))
    {
        printf("join");
    }
    else if (tree_is_type(vertex, STMT_ENTER))
    {
        printf("enter");
    }
    else if (tree_is_type(vertex, STMT_EXIT))
    {
        printf("exit");
    }
    else if (tree_is_type(vertex, DEF_VARIABLE))
    {
        DECLARATION *decl = CAST_TO_DECLARATION(vertex);
        printf("%s", decl->name);
    }
    else
        printf("?%d?", vertex->type);
}


static void edge_printer(NODE *from, NODE *to, void *data)
{
    if (data)
    {
        DFA *dfa = data;
        LIST *output = get_from_hash(dfa->outputs, to, sizeof(void *));
        DAA_SET *set = output->size ? output->items[0] : NULL;
        if (set)
        {
            printf("{");
            HASH_ITERATOR iter;
            for (hash_iterator(set->set, &iter); hash_iterator_valid(&iter);hash_iterator_next(&iter))
            {
                DECLARATION *decl = CAST_TO_DECLARATION(iter.entry->data);
                printf("<font color=\"%s\">%s</font>", get_colour(decl->colour), (char *) iter.entry->key);
                printf(",");
            }
            printf("}");
        }
    }
}


static int graph_sequence = 0;


void print_graph(GRAPH *graph, char *name, void *data)
{
    int i;
    
    graph_sequence++;
    
    printf("subgraph cluster_%s_%d {\n", name, graph_sequence);
    printf("    label=\"%s\"; labelloc=\"t\";\n", name);
    printf("    ranksep=0.1\n");
    printf("    node [shape=\"box\", style=\"filled\"];\n");
    
    /* Vertices. */
    for (i = 0; i < tree_num_children(graph); i++)
    {
        NODE *vertex = tree_get_child(graph, i);
        if (vertex == NULL)
            continue;
        
        printf("    %s_%d_%d [label=<%d. ", name, graph_sequence, i, i);
        vertex_printer(vertex, data);
        printf(">");
        if (tree_is_type(vertex, DEF_VARIABLE))
        {
            DECLARATION *decl = CAST_TO_DECLARATION(vertex);
            printf(", fillcolor=%s", get_colour(decl->colour));
        }
        printf("];\n");
    }
    
    /* Edges. */
    for (i = 0; i < tree_num_children(graph); i++)
    {
        HASH_ENTRY *he;
        NODE *from = tree_get_child(graph, i);
        
        he = find_in_hash(graph->forward, from, sizeof(void *));
        if (he)
        {
            HASH *subhash = he->data;
            HASH_ITERATOR iter;
            for (hash_iterator(subhash, &iter); hash_iterator_valid(&iter);hash_iterator_next(&iter))
            {
                NODE *to = iter.entry->key;
                HASH_ENTRY *he2 = find_in_hash(graph->labels, to, sizeof(void *));
                if (he2)
                {
                    EDGE_TYPE type = (EDGE_TYPE) iter.entry->data;
                    if (type == EDGE_SYMMETRICAL)
                        continue;
                    printf("    %s_%d_%d -> %s_%d_%d [label=<", name, graph_sequence, i, name, graph_sequence, (int) he2->data);
                    if (type & EDGE_YES)
                        printf("Y");
                    if (type & EDGE_NO)
                        printf("N");
                    if (type & EDGE_BACK)
                        printf("B");
                    if (type & EDGE_LOOP)
                        printf("L");
                    edge_printer(from, to, data);
                    printf(">];\n");
                }
            }
        }
    }
    printf("}\n");
}
