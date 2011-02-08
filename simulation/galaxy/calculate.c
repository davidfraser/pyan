#include <math.h>
#include <stdio.h>

#include "galaxy.h"

static double get_distance2(STAR *s1, STAR *s2)
{
    double d2 = 0.0;
    double d = (s1->pos[0] - s2->pos[0]);
    d2 += d*d;
    d = (s1->pos[1] - s2->pos[1]);
    d2 += d*d;
    d = (s1->pos[2] - s2->pos[2]);
    d2 += d*d;
    
    return d2;
}

#define PADDING 1.0

void calculate_force(STAR *s1, STAR *s2, double g, VECTOR force)
{
    double d2 = get_distance2(s1, s2);
    double f = s1->mass * s2->mass * g / (d2 + PADDING);
    double k = f/sqrt(d2);
    VECTOR df;
    
    df[0] = (s2->pos[0] - s1->pos[0])*k;
    df[1] = (s2->pos[1] - s1->pos[1])*k;
    df[2] = (s2->pos[2] - s1->pos[2])*k;
    force[0] += df[0];
    force[1] += df[1];
    force[2] += df[2];
}


#define GRAVITY 6.67E-11


void calculate(GALAXY *galaxy, VECTOR *forces)
{
    int i;
    for (i = 0; i < galaxy->num; i++)
    {
        int j;
        STAR *s1 = galaxy->stars[i];
        
        for (j = 0; j < galaxy->num; j++)
        {
            STAR *s2 = galaxy->stars[j];
            if (i == j)
                continue;
            
            calculate_force(s1, s2, GRAVITY, forces[i]);
        }
    }    
}
