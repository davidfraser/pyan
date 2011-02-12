#include <stdlib.h>

#include "galaxy.h"


float rand_float(float min, float max)
{
    int x = rand();
    float r = max - min;
    return x * r / RAND_MAX + min;
}


double get_distance2(STAR *s1, STAR *s2)
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


void vector_add(VECTOR x, VECTOR y)
{
    x[0] += y[0];
    x[1] += y[1];
    x[2] += y[2];
}


void vector_add_scaled(VECTOR x, VECTOR y, double scale)
{
    x[0] += y[0]*scale;
    x[1] += y[1]*scale;
    x[2] += y[2]*scale;
}


void vector_scale(VECTOR x, double scale)
{
    x[0] *= scale;
    x[1] *= scale;
    x[2] *= scale;
}


GALAXY *create_galaxy()
{
    GALAXY *galaxy = malloc(sizeof(GALAXY));
    galaxy->num = 0;
    galaxy->max = 10;
    galaxy->stars = malloc(sizeof(STAR *) * galaxy->max);
    return galaxy;
}


void destroy_galaxy(GALAXY *galaxy)
{
    int i;
    for (i = 0; i < galaxy->num; i++)
        free(galaxy->stars[i]);
    free(galaxy->stars);
    free(galaxy);
}


void add_star(GALAXY *galaxy, STAR *star)
{
    if (galaxy->num >= galaxy->max)
    {
        galaxy->max = galaxy->max*1.5 + 1;
        galaxy->stars = realloc(galaxy->stars, sizeof(STAR *) * galaxy->max);
    }
    galaxy->stars[galaxy->num++] = star;
}


STAR *create_star()
{
    STAR *star = malloc(sizeof (STAR));
    star->pos[0] = 0.0;
    star->pos[1] = 0.0;
    star->pos[2] = 0.0;
    star->vel[0] = 0.0;
    star->vel[1] = 0.0;
    star->vel[2] = 0.0;
    star->mass = 0.0;
    return star;
}


void destroy_star(STAR *star)
{
    free(star);
}


void dump_star(STAR *star, FILE *f)
{
    fwrite(star->pos, sizeof (star->pos), 1, f);
    fwrite(star->vel, sizeof (star->vel), 1, f);
    fwrite(&star->mass, sizeof (star->mass), 1, f);
}


void dump_galaxy(GALAXY *galaxy, FILE *f)
{
    int i;
    fwrite(&galaxy->num, sizeof (galaxy->num), 1, f);
    for (i = 0; i < galaxy->num; i++)
    {
        dump_star(galaxy->stars[i], f);
    }
}


void update_galaxy(GALAXY *galaxy)
{
    int i;
    
    galaxy->barycentre[0] = 0.0;
    galaxy->barycentre[1] = 0.0;
    galaxy->barycentre[2] = 0.0;
    
    galaxy->mass = 0.0;
    
    for (i = 0; i < galaxy->num; i++)
    {
        STAR *s = galaxy->stars[i];
        vector_add_scaled(galaxy->barycentre, s->pos, s->mass);
        galaxy->mass += s->mass;    
    }

    vector_scale(galaxy->barycentre, 1.0/galaxy->mass);
}


void recentre_galaxy(GALAXY *galaxy)
{
    int i;
    
    for (i = 0; i < galaxy->num; i++)
    {
        STAR *s = galaxy->stars[i];
        vector_add_scaled(s->pos, galaxy->barycentre, -1.0);
    }
}


void blow_up_star(GALAXY *galaxy, STAR *star, int fragments, double velocity)
{
    int i;
    for (i = 0; i < fragments; i++)
    {
        STAR *s = create_star();
        *s = *star;
        s->vel[0] += rand_float(-velocity, velocity);
        s->vel[1] += rand_float(-velocity, velocity);
        s->vel[2] += rand_float(-velocity, velocity);
        s->pos[0] += rand_float(-velocity, velocity);
        s->pos[1] += rand_float(-velocity, velocity);
        s->pos[2] += rand_float(-velocity, velocity);
        s->mass = 1000.0;
        add_star(galaxy, s);
    }
    star->mass = 0.0;
}
