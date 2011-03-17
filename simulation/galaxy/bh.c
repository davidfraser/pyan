#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "galaxy.h"
#include "calculate.h"


typedef struct NODE
{
    VECTOR centre;
    double side;
    STAR *star;
    struct NODE *children[2][2][2];
    STAR star_data;
} NODE;


typedef struct CALC_DATA
{
    double side_limit;
    double threshold;
    NODE *tree;
    NODE *next_node, *max_node;
} BH_CALCULATOR;


static NODE *get_new_node(CALC_DATA *data)
{
    NODE *n = data->next_node;
    
    if (n >= data->max_node)
    {
        fprintf(stderr, "Tree overflow!\n");
        *((char *) 0) = 1;
        exit(1);
    }
    
    data->next_node++;
    return n;
}


static NODE *get_child(CALC_DATA *data, NODE *tree, VECTOR pos)
{
    int c[3];
    NODE *child;

    c[0] = pos[0] >= tree->centre[0];
    c[1] = pos[1] >= tree->centre[1];
    c[2] = pos[2] >= tree->centre[2];
    
    if (tree->children[c[0]][c[1]][c[2]] != NULL)
        return tree->children[c[0]][c[1]][c[2]];
    
    child = get_new_node(data);
    
    child->side = tree->side / 2.0;
    child->centre[0] = tree->centre[0] + (c[0] ? child->side : -child->side)/2.0;
    child->centre[1] = tree->centre[1] + (c[1] ? child->side : -child->side)/2.0;
    child->centre[2] = tree->centre[2] + (c[2] ? child->side : -child->side)/2.0;

    tree->children[c[0]][c[1]][c[2]] = child;
    
    return child;
}


/** Add the positon and mass of s2 into s1. */
static void merge_star(STAR *s1, STAR *s2)
{
    double total_mass = s1->mass + s2->mass;
    int i;
    
    for (i = 0; i < 3; i++)
        s1->pos[i] = (s1->pos[i]*s1->mass + s2->pos[i]*s2->mass) / total_mass;
    
    s1->mass = total_mass;
}


static void insert_star(CALC_DATA *data, NODE *tree, STAR *s)
{
    if (tree->star == NULL)
    {
        //fprintf(stderr, "Insert %p into empty %p.\n", s, tree);
        /* Simple case: node is empty, just make it point to this star. */
        tree->star = s;
    }
    else if (tree->side <= data->side_limit)
    {
        /* Special optimisation: if the tree is small enough, just bin all stars into it. */
        if (tree->star != &tree->star_data)
        {
            merge_star(&tree->star_data, tree->star);
            tree->star = &tree->star_data;
        }
        merge_star(&tree->star_data, s);
    }
    else if (tree->star == &tree->star_data)
    {
        /* Next simple case: node internal, so insert the star into the correct child
           and update the node's star to account for this star. */
        NODE *child = get_child(data, tree, s->pos);
        //fprintf(stderr, "Insert %p into child %p of %p.\n", s, child, tree);
        insert_star(data, child, s);
        merge_star(tree->star, s);
    }
    else
    {
        /* Complicated case: node is external and already occupied.
           Turn it into an internal node, and insert both this star and the node's initial star into one of the children. */
        STAR *star2 = tree->star;
        //fprintf(stderr, "Insert %p alongside %p in %p.\n", s, star2, tree);
        tree->star = &tree->star_data;
        insert_star(data, tree, s);
        insert_star(data, tree, star2);
    }
}


NODE *build_tree(CALC_DATA *data, GALAXY *galaxy)
{
    int max_nodes = galaxy->num * 100;
    data->tree = malloc(sizeof(NODE) * max_nodes);
    data->next_node = data->tree + 1;
    data->max_node = data->tree + max_nodes;    
    int i;
    
    memset(data->tree, 0, sizeof(NODE) * max_nodes);
    
    //fprintf(stderr, "%d %p %p %p\n", max_nodes, tree, next_node, max_node);
    
    data->tree->side = galaxy->radius * 2.0;
    
    for (i = 0; i < galaxy->num; i++)
    {
        STAR *s = galaxy->stars[i];
        if (s->mass == 0.0)
            continue;
        
        insert_star(data, data->tree, s);
        //fprintf(stderr, "Star %d inserted\n", i);
    }
    
    //fprintf(stderr, "Tree built\n");
    return data->tree;
}


static void get_force_from_tree(CALCULATOR *calc, NODE *tree, STAR *s, VECTOR force)
{
    CALC_DATA *data = calc->data;
    double d2;
    
    if (tree == NULL || tree->star == NULL)
        return;
    
    d2 = get_distance2(s, tree->star);
    
    if (tree->star != &tree->star_data || tree->side / sqrt(d2) < data->threshold)
    {
        //fprintf(stderr, "Calculating force between %p and %p\n", tree->star, s);
        calculate.calculate_force(s, tree->star, calc->gravity, force);
    }
    else
    {
        int i, j, k;
        
        for (i = 0; i < 2; i++)
            for (j = 0; j < 2; j++)
                for (k = 0; k < 2; k++)
                    get_force_from_tree(calc, tree->children[i][j][k], s, force);
    }
}


static void calculate_forces(CALCULATOR *calc, NODE *tree, GALAXY *galaxy, VECTOR *forces)
{
    int i;
    
    for (i = 0; i < galaxy->num; i++)
    {
        STAR *s = galaxy->stars[i];
        if (s->mass == 0.0)
            continue;
        
        //fprintf(stderr, "Calculating force for star %p\n", s);
        get_force_from_tree(calc, tree, s, forces[i]);
    }
}



void bh_calculator__calculate(CALCULATOR *calculator, GALAXY *galaxy, VECTOR *forces)
{
    NODE *tree;
    
    tree = build_tree(calculator->data, galaxy);
    
    calculate_forces(calculator, tree, galaxy, forces);
    
    free(tree);
}


static void bh_calculator__destroy(CALCULATOR *calculator)
{
    free(calculator->data);
    free(calculator);
}


CALCULATOR *bh_calculator(void)
{
    CALCULATOR *c = malloc(sizeof(CALCULATOR));
    c->data = malloc(sizeof(CALC_DATA));
    c->data->side_limit = 1000.0;
    c->data->threshold = 0.75;
    c->calculate = bh_calculator__calculate;
    c->destroy = bh_calculator__destroy;
    return c;
}
