#include <stdlib.h>

#include "galaxy.h"

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
