#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "galaxy.h"
#include "calculate.h"


#define PADDING 10.0

void calculate__calculate_force(STAR *s1, STAR *s2, double g, VECTOR force)
{
    if (s1 == s2)
        return;
    
    double d2 = get_distance2(s1, s2);
    double f = s1->mass * s2->mass * g / (d2 + PADDING);
    double k = f/sqrt(d2);
    VECTOR df;
    
    df[0] = (s2->pos[0] - s1->pos[0])*k;
    df[1] = (s2->pos[1] - s1->pos[1])*k;
    df[2] = (s2->pos[2] - s1->pos[2])*k;
    vector_add(force, df);
}

void calculate__apply_forces(GALAXY *g, VECTOR *forces, double timestep)
{
    int i;
    
    for (i = 0; i < g->num; i++)
    {
        STAR *s = g->stars[i];
        if (s->mass == 0.0)
            continue;
        
        vector_add_scaled(s->vel, forces[i], timestep / s->mass);
        vector_add_scaled(s->pos, s->vel, timestep);
    }
}

static void naive_calculator__calculate(CALCULATOR *calculator, GALAXY *galaxy, VECTOR *forces)
{
    int i;
    for (i = 0; i < galaxy->num; i++)
    {
        int j;
        STAR *s1 = galaxy->stars[i];
        
        if (s1->mass == 0.0)
            continue;
        
        for (j = 0; j < galaxy->num; j++)
        {
            STAR *s2 = galaxy->stars[j];
            if (i == j || s2->mass == 0.0)
                continue;
            
            calculate.calculate_force(s1, s2, calculator->gravity, forces[i]);
        }
    }
}

static void naive_calculator__destroy(CALCULATOR *calculator)
{
    free(calculator);
}

CALCULATOR *calculate__naive_calculator(void)
{
    CALCULATOR *c = malloc(sizeof(CALCULATOR));
    c->calculate = naive_calculator__calculate;
    c->destroy = naive_calculator__destroy;
    return c;
}
