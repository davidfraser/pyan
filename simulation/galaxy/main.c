#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "galaxy.h"

extern void bh_calculate(GALAXY *galaxy, VECTOR *forces);
extern void calculate(GALAXY *galaxy, VECTOR *forces);

float rand_float(float min, float max)
{
    int x = rand();
    float r = max - min;
    return x * r / RAND_MAX + min;
}

void calculate_frame(GALAXY *g, double timestep)
{
    int i;
    VECTOR forces[g->num];
    
    memset(forces, 0, sizeof(forces));
    
    calculate(g, forces);
    
    /* Apply forces. */
    for (i = 0; i < g->num; i++)
    {
        STAR *s = g->stars[i];
        s->vel[0] += timestep * forces[i][0] / s->mass;
        s->vel[1] += timestep * forces[i][1] / s->mass;
        s->vel[2] += timestep * forces[i][2] / s->mass;
        
        s->pos[0] += timestep * s->vel[0];
        s->pos[1] += timestep * s->vel[1];
        s->pos[2] += timestep * s->vel[2];
    }
}

/*
5
2.50e11
0.000e00 0.000e00 0.000e00 0.000e00 1.989e30 sun.gif
5.790e10 0.000e00 0.000e00 4.790e04 3.302e23 mercury.gif
1.082e11 0.000e00 0.000e00 3.500e04 4.869e24 venus.gif
1.496e11 0.000e00 0.000e00 2.980e04 5.974e24 earth.gif
2.279e11 0.000e00 0.000e00 2.410e04 6.419e23 mars.gif
*/
static GALAXY *create_solar_system()
{
    int i;
    
    STAR data[] = {
        { { 0.000e00, 0.000e00 }, { 0.000e00, 0.000e00 }, 1.989e30 },
        { { 5.790e10, 0.000e00 }, { 0.000e00, 4.790e04 }, 3.302e23 },
        { { 1.082e11, 0.000e00 }, { 0.000e00, 3.500e04 }, 4.869e24 },
        { { 1.496e11, 0.000e00 }, { 0.000e00, 2.980e04 }, 5.974e24 },
        { { 2.279e11, 0.000e00 }, { 0.000e00, 2.410e04 }, 6.419e23 }
    };
    
    GALAXY *g = create_galaxy();
    
    for (i = 0; i < 5; i++)
    {
        STAR *s = create_star();
        *s = data[i];
        add_star(g, s);
    }
    
    return g;
}

static GALAXY *create_disc_galaxy(double radius, int num)
{
    int i;
    
    GALAXY *g = create_galaxy();
    
    for (i = 0; i < num; i++)
    {
        STAR *s = create_star();
        s->mass = 1E32;
        double a = rand_float(0.0, 2.0*M_PI);
        double r = rand_float(0.0, radius);
        s->pos[0] = r * cos(a);
        s->pos[1] = r * sin(a);
        s->pos[2] = 0.0;
        s->vel[0] = cos(a+M_PI/2.0);
        s->vel[1] = sin(a+M_PI/2.0);
        s->vel[2] = 0.0;
        add_star(g, s);
    }
    
    return g;
}

extern void write_png(const char *file_name, unsigned char *data, int width, int height);

void save_image(GALAXY *g, const char *filename)
{
    int i;
    int width = 512;
    int height = 512;
    static unsigned char buffer[512*512];
    //buffer = malloc(width*height);
    
    //memset(buffer, 0, width*height);
    
    for (i = 0; i < width*height; i++)
    {
        if (buffer[i] > 0)
            buffer[i]--;
    }
    
    for (i = 0; i < g->num; i++)
    {
        STAR *s = g->stars[i];
        int px = (s->pos[0]/2.5E11)*width/2 + width/2;
        int py = (s->pos[1]/2.5E11)*height/2 + height/2;
        if (px >= 0 && px < width && py >= 0 && py < height)
        {
            buffer[py*height + px] = 255;
        }
    }
    
    write_png(filename, buffer, width, height);
    //free(buffer);
}

int main(int argc, char *argv[])
{
    int i;
    FILE *f;
    //GALAXY *g = create_solar_system();
    GALAXY *g = create_disc_galaxy(2.5E11, 1000);
    
    f = fopen("stars.dat", "wb");
    for (i = 0; i < 1000; i++)
    {
        char fn[1000];
        calculate_frame(g, 25000.0);
        dump_galaxy(g, f);
        snprintf(fn, 100, "out%03d.png", i);
        save_image(g, fn);
    }
    fclose(f);
    
    destroy_galaxy(g);
    
    return 0;
}
