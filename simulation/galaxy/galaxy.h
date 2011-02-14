#ifndef GALAXY_H
#define GALAXY_H

#include <stdio.h>

typedef double VECTOR[3];

typedef struct STAR
{
    char *name;
    VECTOR pos;
    VECTOR vel;
    double mass;
    double size;
    unsigned char rgb[3];
} STAR;

typedef struct GALAXY
{
    int num;
    int max;
    STAR **stars;
    double radius;
    VECTOR barycentre;
    double mass;
} GALAXY;


extern float rand_float(float min, float max);
extern double get_distance2(STAR *s1, STAR *s2);
extern void vector_add(VECTOR x, VECTOR y);
extern void vector_add_scaled(VECTOR x, VECTOR y, double scale);

extern GALAXY *create_galaxy();
extern void destroy_galaxy(GALAXY *galaxy);
extern void add_star(GALAXY *galaxy, STAR *star);
extern STAR *create_star();
extern void destroy_star(STAR *star);

extern void dump_star(STAR *star, FILE *f);
extern void dump_galaxy(GALAXY *galaxy, FILE *f);

/** Update the stats, such as barycentre and mass, of the galaxy. */
extern void update_galaxy(GALAXY *galaxy);
extern void recentre_galaxy(GALAXY *galaxy);
extern void blow_up_star(GALAXY *galaxy, STAR *star, int fragments, double velocity);

#endif
