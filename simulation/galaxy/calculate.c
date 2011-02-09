#include <math.h>
#include <stdio.h>

#include "galaxy.h"

#define PADDING 10.0

void calculate_force(STAR *s1, STAR *s2, double g, VECTOR force)
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


#define GRAVITY 6.67E-11


void naive_calculate_all(GALAXY *galaxy, VECTOR *forces)
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
