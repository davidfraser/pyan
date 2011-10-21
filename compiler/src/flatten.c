#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "compiler.h"

static int base = 0;

NODE *flatten_block(MODULE *module, FUNCTION *func, GRAPH *graph, BLOCK *block,
        NODE *predecessor, NODE *exit_node, NODE *loop_start_node, NODE *loop_end_node, EDGE_TYPE edge_type)
{
    int i;
    
    if (block == NULL)
        return predecessor;
    
    if (!tree_is_type(block, STMT_BLOCK))
        error("Attempt to flatten non-block node of type %s", tree_get_name(block));
    
    base++;

    /* Process statements. */
    for (i = 0; i < tree_num_children(block); i++)
    {
        STATEMENT *stmt = tree_get_child(block, i);
        if (stmt == NULL)
        {
            NODE *vertex = CAST_TO_NODE(make_pass(CAST_TO_AST(block)->source_line));
            add_vertex(graph, vertex);
            add_edge(graph, predecessor, vertex, edge_type);
            predecessor = vertex;
        }
        else if (tree_is_type(stmt, STMT_IF))
        {
            NODE *test = CAST_TO_NODE(make_test(tree_get_child(stmt, 0), 0));
            add_vertex(graph, test);
            add_edge(graph, predecessor, test, i == 0 ? edge_type : EDGE_NORMAL);
            NODE *join = CAST_TO_NODE(make_pass(CAST_TO_AST(stmt)->source_line));
            add_vertex(graph, join);
            predecessor = test;
            predecessor = flatten_block(module, func, graph, tree_get_child(stmt, 1), predecessor, exit_node, loop_start_node, loop_end_node, EDGE_YES);
            add_edge(graph, predecessor, join, EDGE_NORMAL);
            predecessor = test;
            predecessor = flatten_block(module, func, graph, tree_get_child(stmt, 2), predecessor, exit_node, loop_start_node, loop_end_node, EDGE_NO);
            if (predecessor)
                add_edge(graph, predecessor, join, EDGE_NORMAL);
            predecessor = join;
        }
        else if (tree_is_type(stmt, STMT_WHILE))
        {
            NODE *test = CAST_TO_NODE(make_test(tree_get_child(stmt, 0), 0));
            add_vertex(graph, test);
            add_edge(graph, predecessor, test, i == 0 ? edge_type : EDGE_NORMAL);
            NODE *join = CAST_TO_NODE(make_pass(CAST_TO_AST(stmt)->source_line));
            add_vertex(graph, join);
            predecessor = test;
            predecessor = flatten_block(module, func, graph, tree_get_child(stmt, 1), predecessor, exit_node, test, join, EDGE_YES | EDGE_LOOP);
            add_edge(graph, predecessor, test, EDGE_BACK);
            add_edge(graph, test, join, EDGE_NO);
            predecessor = join;
        }
        else if (tree_is_type(stmt, STMT_RETURN))
        {
            add_vertex(graph, CAST_TO_NODE(stmt));
            add_edge(graph, predecessor, CAST_TO_NODE(stmt), i == 0 ? edge_type : EDGE_NORMAL);
            add_edge(graph, CAST_TO_NODE(stmt), exit_node, EDGE_NORMAL);
            predecessor = NULL;
        }
        else if (tree_is_type(stmt, STMT_CONTINUE))
        {
            if (loop_start_node == NULL)
                fprintf(stderr, "Continue outside loop on line %d in '%s'!", CAST_TO_AST(stmt)->source_line, func->decl.name);
            else
                add_edge(graph, predecessor, loop_start_node, (i == 0 ? edge_type : EDGE_NORMAL) | EDGE_BACK);
            predecessor = NULL;
        }
        else if (tree_is_type(stmt, STMT_BREAK))
        {
            if (loop_end_node == NULL)
                fprintf(stderr, "Break outside loop on line %d in '%s'!", CAST_TO_AST(stmt)->source_line, func->decl.name);
            else
                add_edge(graph, predecessor, loop_end_node, i == 0 ? edge_type : EDGE_NORMAL);
            predecessor = NULL;
        }
        else if (tree_is_type(stmt, STMT_ASSIGN))
        {
            add_vertex(graph, CAST_TO_NODE(stmt));
            add_edge(graph, predecessor, CAST_TO_NODE(stmt), i == 0 ? edge_type : EDGE_NORMAL);
            predecessor = CAST_TO_NODE(stmt);
        }
        else if (tree_is_type(stmt, STMT_RESTART))
        {
            HASH *subhash = get_from_hash(graph->forward, tree_get_child(graph, 0), sizeof(void *));
            HASH_ITERATOR iter;
            hash_iterator(subhash, &iter);
            NODE *successor = iter.entry->key;
            add_edge(graph, predecessor, successor, EDGE_BACK | (i == 0 ? edge_type : EDGE_NORMAL));
            predecessor = NULL;
        }
        else
            error("Don't know how to flatten node of type %s!", tree_get_name(stmt));
    }
    
    return predecessor;
}


int flatten(MODULE *module, FUNCTION *func)
{
    GRAPH *graph = make_graph(func);
    BLOCK *body = tree_get_child(func, 0);
    
    NODE *entry_node = CAST_TO_NODE(make_enter(CAST_TO_AST(body)->source_line));
    NODE *exit_node = CAST_TO_NODE(make_exit(CAST_TO_AST(body)->source_line));
    NODE *predecessor = entry_node;
    
    add_vertex(graph, entry_node);
    add_vertex(graph, exit_node);
    
    predecessor = flatten_block(module, func, graph, body, predecessor, exit_node, NULL, NULL, EDGE_NORMAL);
    if (predecessor != NULL)
        add_edge(graph, predecessor, exit_node, EDGE_NORMAL);
    
    func->graph = graph;
    
    cleanup_graph(func);
    
    return 1;
}
