#ifndef GALAXY_H
#define GALAXY_H

#include <stdio.h>

typedef double VECTOR[3];

typedef struct STAR
{
    VECTOR pos;
    VECTOR vel;
    double mass;
} STAR;

typedef struct GALAXY
{
    int num;
    int max;
    STAR **stars;
    double radius;
} GALAXY;


extern double get_distance2(STAR *s1, STAR *s2);
extern void vector_add(VECTOR x, VECTOR y);

extern GALAXY *create_galaxy();
extern void destroy_galaxy(GALAXY *galaxy);
extern void add_star(GALAXY *galaxy, STAR *star);
extern STAR *create_star();
extern void destroy_star(STAR *star);

extern void dump_star(STAR *star, FILE *f);
extern void dump_galaxy(GALAXY *galaxy, FILE *f);

#endif
