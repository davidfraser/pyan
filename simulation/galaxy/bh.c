#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "galaxy.h"

typedef struct NODE
{
    VECTOR centre;
    double side;
    STAR *star;
    struct NODE *children[2][2][2];
    STAR star_data;
} NODE;


static NODE *get_child(NODE *tree, NODE **next_node, NODE *max_node, VECTOR pos)
{
    int c[3];
    NODE *child;

    c[0] = pos[0] >= tree->centre[0];
    c[1] = pos[1] >= tree->centre[1];
    c[2] = pos[2] >= tree->centre[2];
    
    if (tree->children[c[0]][c[1]][c[2]] != NULL)
        return tree->children[c[0]][c[1]][c[2]];
    
    if (*next_node >= max_node)
    {
        fprintf(stderr, "Tree overflow!\n");
        *((char *) 0) = 1;
        exit(1);
    }
    
    child = *next_node;
    (*next_node)++;
    
    child->side = tree->side / 2.0;
    child->centre[0] = tree->centre[0] + (c[0] ? child->side : -child->side)/2.0;
    child->centre[1] = tree->centre[1] + (c[1] ? child->side : -child->side)/2.0;
    child->centre[2] = tree->centre[2] + (c[2] ? child->side : -child->side)/2.0;

    tree->children[c[0]][c[1]][c[2]] = child;
    
    return child;
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
        //fprintf(stderr, "Insert %p into empty %p.\n", s, tree);
        /* Simple case: node is empty, just make it point to this star. */
        tree->star = s;
    }
    else if (tree->star == &tree->star_data)
    {
        /* Next simple case: node internal, so insert the star into the correct child
           and update the node's star to account for this star. */
        NODE *child = get_child(tree, next_node, max_node, s->pos);
        //fprintf(stderr, "Insert %p into child %p of %p.\n", s, child, tree);
        insert_star(child, next_node, max_node, s);
        merge_star(tree->star, s);
    }
    else
    {
        /* Complicated case: node is external and already occupied.
           Turn it into an internal node, and insert both this star and the node's initial star into one of the children. */
        STAR *star2 = tree->star;
        //fprintf(stderr, "Insert %p alongside %p in %p.\n", s, star2, tree);
        tree->star = &tree->star_data;
        insert_star(tree, next_node, max_node, s);
        insert_star(tree, next_node, max_node, star2);
    }
}


NODE *build_tree(GALAXY *galaxy)
{
    int max_nodes = galaxy->num * 100;
    NODE *tree = malloc(sizeof(NODE) * max_nodes);
    NODE *next_node = tree + 1;
    NODE *max_node = tree + max_nodes;    
    int i;
    
    memset(tree, 0, sizeof(NODE) * max_nodes);
    
    //fprintf(stderr, "%d %p %p %p\n", max_nodes, tree, next_node, max_node);
    
    tree->side = galaxy->radius * 2.0;
    
    for (i = 0; i < galaxy->num; i++)
    {
        STAR *s = galaxy->stars[i];
        insert_star(tree, &next_node, max_node, s);
        //fprintf(stderr, "Star %d inserted\n", i);
    }
    
    //fprintf(stderr, "Tree built\n");
    return tree;
}


#define THRESHOLD 0.75
#define GRAVITY 6.67E-11

extern void calculate_force(STAR *s1, STAR *s2, double g, VECTOR force);


static void get_force_from_tree(NODE *tree, STAR *s, VECTOR force)
{
    double d2;
    
    if (tree == NULL || tree->star == NULL)
        return;
    
    d2 = get_distance2(s, tree->star);
    
    if (tree->star != &tree->star_data || tree->side / sqrt(d2) < THRESHOLD)
    {
        //fprintf(stderr, "Calculating force between %p and %p\n", tree->star, s);
        calculate_force(s, tree->star, GRAVITY, force);
    }
    else
    {
        int i, j, k;
        
        for (i = 0; i < 2; i++)
            for (j = 0; j < 2; j++)
                for (k = 0; k < 2; k++)
                    get_force_from_tree(tree->children[i][j][k], s, force);
    }
}


static void calculate_forces(NODE *tree, GALAXY *galaxy, VECTOR *forces)
{
    int i;
    
    for (i = 0; i < galaxy->num; i++)
    {
        STAR *s = galaxy->stars[i];
        
        //fprintf(stderr, "Calculating force for star %p\n", s);
        get_force_from_tree(tree, s, forces[i]);
    }
}


void bh_calculate_all(GALAXY *galaxy, VECTOR *forces)
{
    NODE *tree;
    
    tree = build_tree(galaxy);
    
    calculate_forces(tree, galaxy, forces);
    
    free(tree);
}
