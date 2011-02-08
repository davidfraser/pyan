#include <stdlib.h>
#include <stdio.h>

#include "galaxy.h"

typedef struct NODE
{
    VECTOR centre;
    double side;
    STAR *star;
    struct NODE *children[2][2][2];
    STAR star_data;
} NODE;


static void construct_children(NODE *tree, NODE **next_node, NODE *max_node)
{
    if (*next_node + 8 >= max_node)
    {
        fprintf(stderr, "Tree overflow!\n");
        exit(1);
    }
    
    tree->children[0][0][0] = *next_node;
    tree->children[0][0][1] = *next_node + 1;
    tree->children[0][1][0] = *next_node + 2;
    tree->children[0][1][1] = *next_node + 3;
    tree->children[1][0][0] = *next_node + 4;
    tree->children[1][0][1] = *next_node + 5;
    tree->children[1][1][0] = *next_node + 6;
    tree->children[1][1][1] = *next_node + 7;
    *next_node += 8;    
}

static void merge_star(STAR *s1, STAR *s2)
{
    double total_mass = s1->mass + s2->mass;
    int i;
    
    for (i = 0; i < 3; i++)
        s1->pos[i] = (s1->pos[i]*s1->mass + s2->pos[i]*s2->mass) / total_mass;
    
    s1->mass = total_mass;
}

static void insert_star(NODE *tree, NODE **next_node, NODE *max_node, STAR *s)
{
    if (tree->star == NULL)
    {
        /* Simple case: node is empty, just make it point to this star. */
        tree->star = s;
    }
    else if (tree->children[0][0][0] != NULL)
    {
        /* Next simple case: node has children, so insert the star into the correct child
           and update the node's star to account for this star. */
        NODE *child = tree->children[s->pos[0] < tree->centre[0]][s->pos[1] < tree->centre[1]][s->pos[2] < tree->centre[2]];
        insert_star(child, next_node, max_node, s);
        merge_star(tree->star, s);
    }
    else
    {
        /* Complicated case: node doesn't have children but is already occupied.
           Construct all children, and move both this star and the node's start into one of the children. */
        STAR *star2 = tree->star;
        tree->star = &tree->star_data;
        construct_children(tree, next_node, max_node);
        insert_star(tree, next_node, max_node, s);
        insert_star(tree, next_node, max_node, star2);
    }
}

NODE *build_tree(GALAXY *galaxy)
{
    int max_nodes = galaxy->num * 10;
    NODE *tree = malloc(sizeof(NODE) * max_nodes);
    NODE *next_node = tree + 1;
    NODE *max_node = tree + max_nodes;
    
    int i;
    
    for (i = 0; i < galaxy->num; i++)
    {
        STAR *s = galaxy->stars[i];
        insert_star(tree, &next_node, max_node, s);
    }
    
    return tree;
}

static void calculate_forces(NODE *tree, GALAXY *galaxy, VECTOR *forces)
{
    
}

void bh_calculate(GALAXY *galaxy, VECTOR *forces)
{
    NODE *tree;
    
    tree = build_tree(galaxy);
    
    calculate_forces(tree, galaxy, forces);
    
    free(tree);
}
