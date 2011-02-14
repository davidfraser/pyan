#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "galaxy.h"

extern void bh_calculate_all(GALAXY *galaxy, VECTOR *forces);
extern void naive_calculate_all(GALAXY *galaxy, VECTOR *forces);

void calculate_frame(GALAXY *g, double timestep)
{
    int i;
    VECTOR forces[g->num];
    
    memset(forces, 0, sizeof(forces));
    
    //naive_calculate_all(g, forces);
    bh_calculate_all(g, forces);
    
    /* Apply forces. */
    for (i = 0; i < g->num; i++)
    {
        STAR *s = g->stars[i];
        if (s->mass == 0.0)
            continue;
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
        { "Sol", { 0.000e00, 0.000e00 }, { 0.000e00, 0.000e00 }, 1.989e30 },
        { "Mercury", { 5.790e10, 0.000e00 }, { 0.000e00, 4.790e04 }, 3.302e23 },
        { "Venus", { 1.082e11, 0.000e00 }, { 0.000e00, 3.500e04 }, 4.869e24 },
        { "Earth", { 1.496e11, 0.000e00 }, { 0.000e00, 2.980e04 }, 5.974e24 },
        { "Mars", { 2.279e11, 0.000e00 }, { 0.000e00, 2.410e04 }, 6.419e23 },
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
        { "Sol", { -6.185971372636502E+08,  7.053427694221177E+07,  2.338221077370279E+06 }, {  2.007312008802111E+00, -1.050997984989123E+01, -2.364368911319387E-02 },  1.98910E+30, 1.0, { 255,255,0 } },
        { "Mercury", {  1.284273743509015E+10, -6.652395322379692E+10, -6.673910195541095E+09 }, {  3.798138838776709E+04,  1.213699750496125E+04, -2.492355276317566E+03 },  3.30200E+23, 1.0, { 255,255,255 } },
        { "Venus", { -1.007411018134462E+11, -3.996141895535587E+10,  5.232264116797000E+09 }, {  1.276627109059595E+04, -3.268876952904768E+04, -1.184370543035742E+03 },  4.86850E+24, 1.0, { 0,255,0 } },
        { "Earth", { -1.132094265214519E+11,  9.548289411980477E+10, -3.369847273975611E+05 }, { -1.973662621796277E+04, -2.285956152047924E+04,  1.083328102204462E+00 },  5.97360E+24, 1.0, { 0,0,255 } },
        { "Moon", { -1.128423333235537E+11,  9.564765604362176E+10,  3.290122818410397E+07 }, { -2.015398219667083E+04, -2.198526562956678E+04, -3.467219883073369E+01 }, 734.9E+20, 1.73753E+06, { 255,255,255 } },
        { "Mars", {  1.537029064731368E+11, -1.385220649320696E+11, -6.691185912844039E+09 }, {  1.710943707271193E+04,  2.009092334165851E+04,  1.110321260857638E+00 },  6.41850E+23, 1.0, { 255,0,0 } },
        { "Phobos", {  1.536959115224088E+11, -1.385277512615332E+11, -6.688139217549749E+09 }, {  1.821267511026613E+04,  1.840628710864996E+04, -6.153766657189825E+02 },  1.08E+20, 1.11E+04, { 255,255,255 } },
        { "Deimos", {   1.537228570552382E+11, -1.385137585369931E+11, -6.700297482944936E+09 }, {  1.672568339339906E+04,  2.134989237437802E+04,  3.082438185365639E+02, },  1.80E+20, 6.0E+3, { 255,255,255 } },
        { "Jupiter", {  7.256525012200071E+11,  1.426602644693087E+11, -1.684232596585476E+10 }, { -2.678138016678334E+03,  1.344328751121466E+04,  4.061932828932413E+00 },  1.89813E+27, 1.0, { 255,255,255 } },
        { "Io", { 7.260689038329406E+11,  1.425983883181777E+11, -1.683880033370411E+10 }, { -6.782917579410297E+01,  3.060258721665560E+04,  6.585420109319209E+02 }, 8.933E+22, 1.8213E+06, { 255,255,255 } },
        { "Europa", {  7.261610834953812E+11,  1.422150272943564E+11, -1.685660497491473E+10 }, {  6.257529127494619E+03,  2.373721511394373E+04,  5.010529147104954E+02 },  4.797E+22, 1.565E+06, { 255,255,255 } },
        { "Ganymede", {  7.247560266685690E+11,  1.420761913792518E+11, -1.687564494540769E+10 }, { 3.288648174409432E+03,  4.350553200754702E+03, -2.624165752182233E+02 }, 1.482E+20 ,2.634E+06, { 255,255,255 } },
        { "Callisto", {  7.252252733813124E+11,  1.444960450288815E+11, -1.678917403061590E+10 }, { -1.066810523255353E+04,  1.164941341168039E+04, -1.601159653020332E+02 }, 1.076E+20, 2.403E+06, { 255,255,255 } },
        { "Saturn", { -1.403963658870086E+12, -2.965957930218173E+11,  6.102786096438922E+10 }, {  1.476330028656222E+03, -9.471913636904839E+03,  1.061583054386461E+02 },  5.68319E+26, 1.0, { 255,255,255 } },
        { "Uranus", {  3.004292714643044E+12, -2.462729937283149E+09, -3.893155014788460E+10 }, { -4.413452596504940E-05,  6.492447331148721E+03,  2.473039486975681E+01 },  8.68103E+25, 1.0, { 255,255,255 } },
        { "Neptune", {  3.814204544285126E+12, -2.367252636733651E+12, -3.915194618599451E+10 }, {  2.829641479452969E+03,  4.650563551022861E+03, -1.602996079114389E+02 },  1.02410E+26, 1.0, { 255,255,255 } },
        { "Pluto", {  4.356646254808469E+11, -4.748243320024902E+12,  3.820704462138057E+11 }, {  5.520933631414783E+03, -5.703864314295275E+02, -1.555800005493817E+03 },  1.31400E+22, 1.0, { 255,255,255 } }
    };
    
    GALAXY *g = create_galaxy();
    
    for (i = 0; i < sizeof(data)/sizeof(STAR); i++)
    {
        STAR *s = create_star();
        *s = data[i];
        add_star(g, s);
    }
    
    //blow_up_star(g, g->stars[9], 10, 10.0);
    
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

void save_image(GALAXY *g, const char *filename, int save)
{
    #define WIDTH 512
    #define HEIGHT 512
    
    int i;
    int width = WIDTH;
    int height = HEIGHT;
    static unsigned char buffer[3*WIDTH*HEIGHT];
    //buffer = malloc(width*height);
    
    //memset(buffer, 0, width*height);
    
    for (i = 0; i < 3*width*height; i++)
    {
        if (buffer[i] > 10)
            buffer[i]--;
    }
    
    double zoom = 20.0;
    double focus_x = 0.0;
    double focus_y = 0.0;
    
    for (i = 0; i < g->num; i++)
    {
        STAR *s = g->stars[i];
        if (s->size == 0.0)
            continue;
        int px = ((s->pos[0] - focus_x)/g->radius) * zoom  *width/2 + width/2;
        int py = ((s->pos[1] - focus_y)/g->radius) * zoom * height/2 + height/2;
        if (px >= 0 && px < width && py >= 0 && py < height)
        {
            buffer[3*(py*height + px)] = s->rgb[0];
            buffer[3*(py*height + px)+1] = s->rgb[1];
            buffer[3*(py*height + px)+2] = s->rgb[2];
        }
    }
    
    if (save)
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
    
    int num_frames = 1000;
    int calcs_per_frame = 10;
    double time_per_frame = SECONDS_PER_YEAR/1000;
    int frames_per_image = 1;
    
    f = fopen("stars.dat", "wb");
    for (i = 0; i < num_frames; i++)
    {
        char fn[1000];
        int j;
        for (j = 0; j < calcs_per_frame; j++)
            calculate_frame(g, time_per_frame/calcs_per_frame);
        update_galaxy(g);
        printf("%f %f %f\n", g->barycentre[0]/time_per_frame, g->barycentre[1]/time_per_frame, g->barycentre[2]/time_per_frame);
        recentre_galaxy(g);
        //fprintf(stderr, "Barycentre %f,%f,%f; mass %f; movement %f\n", g->barycentre[0], g->barycentre[1], g->barycentre[2], g->mass, (bcx - g->barycentre[1])/100/10000);
        
        dump_galaxy(g, f);
        snprintf(fn, sizeof(fn), "img/out%05d.png", i / frames_per_image);
        save_image(g, fn, i % frames_per_image == 0);
    }
    fclose(f);
    
    destroy_galaxy(g);
    
    return 0;
}
