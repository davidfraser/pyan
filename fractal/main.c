#include "fractal.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>

#include <SDL.h>
#include <SDL_ttf.h>

#ifdef WIN32
    #define snprintf sprintf_s
    #define FONT_PATH "c:/windows/fonts/arial.ttf"
#else
    #define FONT_PATH "/usr/share/fonts/truetype/msttcorefonts/arial.ttf"
#endif


static struct {
    char *name;
    void (* init)(int w, int h);
    void (* restart)(MFUNC mfunc);
    void (* update)(void);
} modes[] = {
    { "SIMPLE", simple_init, simple_restart, simple_update },
    { "PARALLEL", parallel_init, parallel_restart, parallel_update },
    { "TRACE", trace_init, trace_restart, trace_update },
    { NULL }
};
static int num_modes;

static struct {
    char *name;
    MFUNC *mfunc;
} mfunc_modes[] = {
    { "LOOP", mfunc_loop },
    { "LOOP_FLOAT", mfunc_loop_float },
    { "LOOP_INT", mfunc_loop_int },
    { "SIMD", mfunc_simd },
    { NULL }
};
static int num_mfunc_modes;


void DrawPixel(SDL_Surface *screen, Uint8 R, Uint8 G, Uint8 B, int x, int y)
{
    Uint32 color = SDL_MapRGB(screen->format, R, G, B);

    switch (screen->format->BytesPerPixel) {
        case 1: { /* Assuming 8-bpp */
            Uint8 *bufp;

            bufp = (Uint8 *)screen->pixels + y*screen->pitch + x;
            *bufp = color;
        }
        break;

        case 2: { /* Probably 15-bpp or 16-bpp */
            Uint16 *bufp;

            bufp = (Uint16 *)screen->pixels + y*screen->pitch/2 + x;
            *bufp = color;
        }
        break;

        case 3: { /* Slow 24-bpp mode, usually not used */
            Uint8 *bufp;

            bufp = (Uint8 *)screen->pixels + y*screen->pitch + x;
            *(bufp+screen->format->Rshift/8) = R;
            *(bufp+screen->format->Gshift/8) = G;
            *(bufp+screen->format->Bshift/8) = B;
        }
        break;

        case 4: { /* Probably 32-bpp */
            Uint32 *bufp;

            bufp = (Uint32 *)screen->pixels + y*screen->pitch/4 + x;
            *bufp = color;
        }
        break;
    }
}

void ReadPixel(SDL_Surface *screen, Uint8 *R, Uint8 *G, Uint8 *B, int x, int y)
{
    Uint8 A;

    switch (screen->format->BytesPerPixel) {
        case 1: { /* Assuming 8-bpp */
            Uint8 *bufp;

            bufp = (Uint8 *)screen->pixels + y*screen->pitch + x;
            SDL_GetRGBA(*bufp, screen->format, R, G, B, &A);
        }
        break;

        case 2: { /* Probably 15-bpp or 16-bpp */
            Uint16 *bufp;

            bufp = (Uint16 *)screen->pixels + y*screen->pitch/2 + x;
            SDL_GetRGBA(*bufp, screen->format, R, G, B, &A);
        }
        break;

        case 3: { /* Slow 24-bpp mode, usually not used */
            Uint8 *bufp;

            bufp = (Uint8 *)screen->pixels + y*screen->pitch + x;
            *R = *(bufp+screen->format->Rshift/8);
            *G = *(bufp+screen->format->Gshift/8);
            *B = *(bufp+screen->format->Bshift/8);
        }
        break;

        case 4: { /* Probably 32-bpp */
            Uint32 *bufp;

            bufp = (Uint32 *)screen->pixels + y*screen->pitch/4 + x;
            SDL_GetRGBA(*bufp, screen->format, R, G, B, &A);
        }
        break;
    }
}

void hsl_to_colour(double h, double s, double l, SDL_Color *colour)
{
    double c = (1 - fabs(2*l - 1)) * s;
    double hp = h*6.0;
    double hpm2;
    double x;
    double m = l - c/2.0;
    double r1 = 0, g1 = 0, b1 = 0;

    hpm2 = hp;
    while (hpm2 >= 2.0)
        hpm2 -= 2.0;
    
    x = c*(1 - fabs(hpm2 - 1));

    if (hp < 1.0)
    {
        r1 = c;
        g1 = x;
    }
    else if (hp < 2.0)
    {
        r1 = x;
        g1 = c;
    }
    else if (hp < 3.0)
    {
        g1 = c;
        b1 = x;
    }
    else if (hp < 4.0)
    {
        g1 = x;
        b1 = c;
    }
    else if (hp < 5.0)
    {
        r1 = x;
        b1 = c;
    }
    else
    {
        r1 = c;
        b1 = x;
    }

    colour->r = (int) (255 * (r1+m));
    colour->g = (int) (255 * (g1+m));
    colour->b = (int) (255 * (b1+m));
}

void error()
{
    fprintf(stderr, "SDL error: %s\n", SDL_GetError());
        exit(1);
}


double centrex, centrey;
double scale;
static int screen_width;
static int screen_height;
static int width;
static int height;
static int max;
int max_iterations;
int pixels_done;
static SDL_Surface *display = NULL;
static float *buffer;
char *status = "?";
static clock_t start_time, end_time;
static int current_mode = 0;
static int current_mfunc_mode = 0;


void set_pixel(int x, int y, float val)
{
    SDL_Color col;
    int x1 = x/2;
    int y1 = y/2;
    double f, g;

    buffer[y*width + x] = val;
    val = buffer[y*width + x] + buffer[(y^1)*width + x] + buffer[y*width + (x^1)] + buffer[(y^1)*width + (x^1)];
    val /= 4.0;

    //f = sqrt(val) / sqrt((double) max_iterations);
    //hsl_to_colour(0, 0, f, &col);
    //col.r = (int) val % 256;
    //col.g = 255;
    f = log(val) / log((double) max_iterations);
    g = sqrt(val) / sqrt((double) max_iterations);
    hsl_to_colour(g, 0.5, f, &col);

    DrawPixel(display, col.r, col.g, col.b, x1, y1);
    pixels_done++;
}

float do_pixel(int x, int y)
{
    double px = (x - width/2.0)*scale + centrex;
    double py = (y - height/2.0)*scale + centrey;
    double fx, fy;
    float val;

    int k = mfunc_direct(px, py, max_iterations, &fx, &fy);

    if (k == 0)
    {
        val = 0.0;
    }
    else
    {
        float z = sqrt(fx*fx + fy*fy);
        val = (float) k - log(log(z))/log(2.0);
    }

    set_pixel(x, y, val);

    return val;
}

void fade_screen()
{
    int i, j;
    for (i = 0; i < screen_height; i++)
        for (j = 0; j < screen_width; j++)
        {
            SDL_Color col;
            ReadPixel(display, &col.r, &col.g, &col.b, j, i);
            DrawPixel(display, col.r/2, col.g/2, col.b/2, j, i);
        }

}

void restart()
{
    modes[current_mode].restart(mfunc_modes[current_mfunc_mode].mfunc);
    pixels_done = 0;
    start_time = clock();
}

static int benchmark = 0;
static int benchmark_loops = 1;

static void parse_args(int argc, char *argv[])
{
    int i;
    
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--benchmark") == 0)
        {
            benchmark = 1;
        }
        else if (strcmp(argv[i], "--mode") == 0)
        {
            i++;
            if (i >= argc)
            {
                fprintf(stderr, "--mode argument needs to be followed by a mode name\n");
                exit(1);
            }
            while (modes[current_mode].name)
            {
                if (strcmp(modes[current_mode].name, argv[i]) == 0)
                    break;
                current_mode++;
            }
            if (!modes[current_mode].name)
            {
                fprintf(stderr, "No such mode: %s\n", argv[i]);
                exit(1);
            }
        }
        else if (strcmp(argv[i], "--mfunc") == 0)
        {
            i++;
            if (i >= argc)
            {
                fprintf(stderr, "--mfunc argument needs to be followed by a mfunc mode name\n");
                exit(1);
            }
            while (mfunc_modes[current_mfunc_mode].name)
            {
                if (strcmp(mfunc_modes[current_mfunc_mode].name, argv[i]) == 0)
                    break;
                current_mfunc_mode++;
            }
            if (!mfunc_modes[current_mfunc_mode].name)
            {
                fprintf(stderr, "No such mfunc mode: %s\n", argv[i]);
                exit(1);
            }
        }
        else if (strcmp(argv[i], "--depth") == 0)
        {
            i++;
            if (i >= argc)
            {
                fprintf(stderr, "--depth argument needs to be followed by a natural number\n");
                exit(1);
            }
            max_iterations = atoi(argv[i]);
        }
        else if (strcmp(argv[i], "--loops") == 0)
        {
            i++;
            if (i >= argc)
            {
                fprintf(stderr, "--loops argument needs to be followed by a natural number\n");
                exit(1);
            }
            benchmark_loops = atoi(argv[i]);
        }
        else
        {
            fprintf(stderr, "Unrecognised command: %s\n", argv[i]);
            exit(1);
        }
    }
}


#define BENCHMARK_SIZE 1000


void do_benchmark(void)
{
    int i;
    int average_pps;
    char filename[1000];
    
    centrex = -0.754682, centrey = 0.055260;
    screen_width = BENCHMARK_SIZE;
    screen_height = BENCHMARK_SIZE;
    width = screen_width*2;
    height = screen_height*2;
    scale = 0.000732/screen_height;

    display = SDL_CreateRGBSurface(SDL_SWSURFACE, screen_width, screen_height, 32, 0, 0, 0, 0);
    
    buffer = (float *) malloc(sizeof(int) * width * height);
    memset(buffer, 0, sizeof(int) * width * height);

    modes[current_mode].init(width, height);
    
    printf("Starting benchmark of mode %s, size %dx%d, max depth %d\n", modes[current_mode].name, width, height, max_iterations);
    
    average_pps = 0;
    for (i = 1; i <= benchmark_loops; i++)
    {
        float seconds;
        int pixels_per_second;
        
        restart();

        while (pixels_done < width*height)
        {
            modes[current_mode].update();
        }

        end_time = clock();
        seconds = (end_time - start_time) / (float) CLOCKS_PER_SEC;
        pixels_per_second = (seconds > 0) ? pixels_done/seconds : 0;
        
        printf("Benchmark iteration %d, PPS was %d\n", i, pixels_per_second);
        average_pps += pixels_per_second;
    }

    snprintf(filename, sizeof(filename), "%s_%dx%d_%d.bmp", modes[current_mode].name, width, height, max_iterations);
    SDL_SaveBMP(display, filename);
    SDL_FreeSurface(display);

    average_pps /= benchmark_loops;
    printf("Benchmark finished, average PPS was %d\n", average_pps);
}

#define FULL_SCREEN 1

int main(int argc, char *argv[])
{
    SDL_Event evt;
    int running = 1;
    TTF_Font *font;
    const SDL_VideoInfo* video_info;
    int save_num = 0;

    num_modes = 0;
    while (modes[num_modes].name != NULL)
        num_modes++;
    
    num_mfunc_modes = 0;
    while (mfunc_modes[num_mfunc_modes].name != NULL)
        num_mfunc_modes++;
    
    max = 0;
    max_iterations = 256;

    parse_args(argc, argv);
    
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        error();
    }

    if (TTF_Init() < 0) {
        error();
    }

    font = TTF_OpenFont(FONT_PATH, 16);
    if (!font)
        error();

    if (benchmark)
    {
        do_benchmark();
        TTF_Quit();            
        SDL_Quit();
        exit(1);
    }

#if FULL_SCREEN
    display = SDL_SetVideoMode(0, 0, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN);
#else
    display = SDL_SetVideoMode(400, 400, 32, SDL_HWSURFACE | SDL_DOUBLEBUF);
#endif
    if(display == NULL) {
        error();
    }
    
    video_info = SDL_GetVideoInfo();
    
    centrex = 0.0, centrey = 0.0;
    screen_width = video_info->current_w;
    screen_height = video_info->current_h;
    width = screen_width*2;
    height = screen_height*2;
    scale = 1.5/screen_height;

    buffer = (float *) malloc(sizeof(int) * width * height);
    memset(buffer, 0, sizeof(int) * width * height);

    modes[current_mode].init(width, height);
    restart();

    while (running)
    {
        while (SDL_PollEvent(&evt))
        {
            if (evt.type == SDL_QUIT)
                running = 0;
            else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_ESCAPE)
                running = 0;
            else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_1)
            {
                max = !max;
                max_iterations = max ? (256*256) : 256;
                fade_screen();
                restart();
            }
            else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_2)
            {
                fade_screen();
                if (evt.key.keysym.mod & KMOD_SHIFT)
                {
                    current_mode--;
                    if (current_mode < 0)
                        current_mode = num_modes - 1;
                }
                else
                {
                    current_mode++;
                    if (current_mode >= num_modes)
                        current_mode = 0;
                }
                modes[current_mode].init(width, height);
                restart();
            }
            else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_3)
            {
                fade_screen();
                if (evt.key.keysym.mod & KMOD_SHIFT)
                {
                    current_mfunc_mode--;
                    if (mfunc_modes[current_mfunc_mode].name < 0)
                        current_mfunc_mode = num_mfunc_modes - 1;
                }
                else
                {
                    current_mfunc_mode++;
                    if (current_mfunc_mode >= num_mfunc_modes)
                        current_mfunc_mode = 0;
                }
                modes[current_mode].init(width, height);
                restart();
            }
            else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_F12)
            {
                char buffer[100];
                snprintf(buffer, sizeof(buffer), "save%04d.bmp", save_num);
                save_num++;
                SDL_SaveBMP(display, buffer);
            }
            else if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == 1)
            {
                centrex = (evt.button.x - screen_width/2.0)*scale*2 + centrex;
                centrey = (evt.button.y - screen_height/2.0)*scale*2 + centrey;
                scale = scale * M_SQRT1_2;
                fade_screen();
                restart();
            }
            else if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == 3)
            {
                centrex = (evt.button.x - screen_width/2.0)*scale*2 + centrex;
                centrey = (evt.button.y - screen_height/2.0)*scale*2 + centrey;
                scale = scale / M_SQRT1_2;
                fade_screen();
                restart();
            }
        }

        if ( SDL_MUSTLOCK(display) ) {
            if ( SDL_LockSurface(display) < 0 ) {
                error();
            }
        }

        modes[current_mode].update();

        {
            SDL_Color white = { 255, 255, 255 };
            SDL_Color black = { 0, 0, 0 };
            char buffer[1000];
            SDL_Surface *txt;
            SDL_Rect dest = { 0, 0 };
            float seconds;
            int pixels_per_second;

            if (pixels_done < width*height)
                end_time = clock();
            seconds = (end_time - start_time) / (float) CLOCKS_PER_SEC;
            pixels_per_second = (seconds > 0) ? pixels_done/seconds : 0;

            snprintf(buffer, sizeof(buffer), "mode=%s, mfunc=%s, depth=%d, done=%d/%d, PPS=%d, cx,cy=%f,%f, scale=%f, status=%s     ",
                    modes[current_mode].name, mfunc_modes[current_mfunc_mode].name, max_iterations, pixels_done, width*height, pixels_per_second, centrex, centrey, scale*screen_height, status);
            txt = TTF_RenderText(font, buffer, white, black);
            dest.w = txt->w;
            dest.h = txt->h;

            SDL_BlitSurface(txt, NULL, display, &dest);
            SDL_FreeSurface(txt);
        }

        if ( SDL_MUSTLOCK(display) ) {
            SDL_UnlockSurface(display);
        }

        SDL_UpdateRect(display, 0, 0, screen_width, screen_height);

        if (pixels_done >= width*height)
            SDL_Delay(100);
    }

    TTF_Quit();
        
    SDL_Quit();

    exit(0);
}
