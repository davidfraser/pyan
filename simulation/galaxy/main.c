#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "galaxy.h"

extern void bh_calculate_all(GALAXY *galaxy, VECTOR *forces);
extern void naive_calculate_all(GALAXY *galaxy, VECTOR *forces);

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
    
    naive_calculate_all(g, forces);
    //bh_calculate_all(g, forces);
    
    /* Apply forces. */
    for (i = 0; i < g->num; i++)
    {
        STAR *s = g->stars[i];
        vector_add_scaled(s->vel, forces[i], timestep / s->mass);
        vector_add_scaled(s->pos, s->vel, timestep);
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
        { { 2.279e11, 0.000e00 }, { 0.000e00, 2.410e04 }, 6.419e23 },
    };
    
    GALAXY *g = create_galaxy();
    
    for (i = 0; i < sizeof(data)/sizeof(STAR); i++)
    {
        STAR *s = create_star();
        *s = data[i];
        add_star(g, s);
    }
    
    g->radius = 2.5E11;
    
    return g;
}


static GALAXY *create_solar_system_2()
{
    int i;
    
    STAR data[] = {
        { { -6.185971372636502E+08,  7.053427694221177E+07,  2.338221077370279E+06 }, {  2.007312008802111E-06, -1.050997984989123E-05, -2.364368911319387E-08 },  1.98910E30 },
        { {  1.284273743509015E+10, -6.652395322379692E+10, -6.673910195541095E+09 }, {  3.798138838776709E+04,  1.213699750496125E+04, -2.492355276317566E+03 },  3.30200E23 },
        { { -1.007411018134462E+11, -3.996141895535587E+10,  5.232264116797000E+09 }, {  1.276627109059595E+04, -3.268876952904768E+04, -1.184370543035742E+03 },  4.86850E24 },
        { { -1.132094265214519E+11,  9.548289411980477E+10, -3.369847273975611E+05 }, { -1.973662621796277E+04, -2.285956152047924E+04,  1.083328102204462E-06 },  5.97360E24 },
        { {  1.537029064731368E+11, -1.385220649320696E+11, -6.691185912844039E+09 }, {  1.710943707271193E+04,  2.009092334165851E+04,  1.110321260857638E-06 },  6.41850E23 },
        { {  7.256525012200071E+11,  1.426602644693087E+11, -1.684232596585476E+10 }, { -2.678138016678334E+03,  1.344328751121466E+04,  4.061932828932413E-06 },  1.89813E27 },
        { { -1.403963658870086E+12, -2.965957930218173E+11,  6.102786096438922E+10 }, {  1.476330028656222E+03, -9.471913636904839E+03,  1.061583054386461E-04 },  5.68319E26 },
        { {  3.004292714643044E+12, -2.462729937283149E+09, -3.893155014788460E+10 }, { -4.413452596504940E-05,  6.492447331148721E+03,  2.473039486975681E-05 },  8.68103E25 },
        { {  3.814204544285126E+12, -2.367252636733651E+12, -3.915194618599451E+10 }, {  2.829641479452969E+03,  4.650563551022861E+03, -1.602996079114389E-04 },  1.02410E26 },
        { {  4.356646254808469E+11, -4.748243320024902E+12,  3.820704462138057E+11 }, {  5.520933631414783E+03, -5.703864314295275E-04, -1.555800005493817E+03 },  1.31400E22 }
    };
    
    GALAXY *g = create_galaxy();
    
    for (i = 0; i < sizeof(data)/sizeof(STAR); i++)
    {
        STAR *s = create_star();
        *s = data[i];
        add_star(g, s);
    }
    
    g->radius = 7E12;
    
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
    
    g->radius = radius;
    
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
        if (buffer[i] > 100)
            buffer[i]--;
    }
    
    for (i = 0; i < g->num; i++)
    {
        STAR *s = g->stars[i];
        int px = (s->pos[0]/g->radius)*width/2 + width/2;
        int py = (s->pos[1]/g->radius)*height/2 + height/2;
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
    GALAXY *g = create_solar_system_2();
    //GALAXY *g = create_disc_galaxy(2.5E11, 1000);
    
    #define SECONDS_PER_YEAR 365.242199*24*3600
    
    f = fopen("stars.dat", "wb");
    for (i = 0; i < 250*4; i++)
    {
        char fn[1000];
        int j;
        for (j = 0; j < 100; j++)
            calculate_frame(g, SECONDS_PER_YEAR/100/4);
        double bcx = g->barycentre[1];
        update_galaxy(g);
        //fprintf(stderr, "Barycentre %f,%f,%f; mass %f; movement %f\n", g->barycentre[0], g->barycentre[1], g->barycentre[2], g->mass, (bcx - g->barycentre[1])/100/10000);
        
        dump_galaxy(g, f);
        snprintf(fn, 100, "img/out%05d.png", i / 4);
        save_image(g, fn);
    }
    fclose(f);
    
    destroy_galaxy(g);
    
    return 0;
}
