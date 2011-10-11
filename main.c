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


extern void parallel_init(int w, int h);
extern void parallel_restart(void);
extern void parallel_update(void);

extern void trace_init(int w, int h);
extern void trace_restart(void);
extern void trace_update(void);

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

int mfunc(double cx, double cy, int max_iterations, double *fx, double *fy)
{
	int i = 0;
	double zr = 0.0, zi = 0.0;

	while (i < max_iterations && zr*zr + zi*zi < 2.0*2.0)
	{
		double t = zr;
		zr = zr*zr - zi*zi + cx;
		zi = 2*t*zi + cy;
		i++;
	}
	*fx = zr;
	*fy = zi;

	if (zr*zr + zi*zi < 2.0*2.0)
		return 0;

	return i;
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


static double centrex, centrey;
static double scale;
static int screen_width;
static int screen_height;
static int width;
static int height;
static int max;
int max_iterations;
static int pixels_done;
static SDL_Surface *display;
static float *buffer;
char *status = "?";
static clock_t start_time, end_time;


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

int do_pixel(int x, int y)
{
	double px = (x - width/2.0)*scale + centrex;
	double py = (y - height/2.0)*scale + centrey;
	double fx, fy;
	float val;

	int k = mfunc(px, py, max_iterations, &fx, &fy);

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

	return k;
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

int main(int argc, char *argv[])
{
    SDL_Event evt;
	int running = 1;
	TTF_Font *font;
    const SDL_VideoInfo* video_info;
    int save_num = 0;

	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        error();
    }

	if (TTF_Init() < 0) {
		error();
	}

	font = TTF_OpenFont(FONT_PATH, 16);
	if (!font)
		error();

	display = SDL_SetVideoMode(0, 0, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN);
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
    max = 0;
    max_iterations = 256;    

	buffer = (float *) malloc(sizeof(int) * width * height);
	memset(buffer, 0, sizeof(int) * width * height);

	trace_init(width, height);
	pixels_done = 0;
	start_time = clock();

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
				trace_restart();
				pixels_done = 0;
				max = !max;
				max_iterations = max ? (256*256) : 256;
				start_time = clock();
			}
			else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_F12)
			{
                char buffer[100];
                snprintf(buffer, sizeof(buffer), "save%d.bmp", save_num);
                save_num++;
                SDL_SaveBMP(display, buffer);
			}
			else if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == 1)
			{
				centrex = (evt.button.x - screen_width/2.0)*scale*2 + centrex;
				centrey = (evt.button.y - screen_height/2.0)*scale*2 + centrey;
				scale = scale * M_SQRT1_2;
				trace_restart();
				pixels_done = 0;
				fade_screen();
				start_time = clock();
			}
			else if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == 3)
			{
				centrex = (evt.button.x - screen_width/2.0)*scale*2 + centrex;
				centrey = (evt.button.y - screen_height/2.0)*scale*2 + centrey;
				scale = scale / M_SQRT1_2;
				trace_restart();
				pixels_done = 0;
				fade_screen();
				start_time = clock();
			}
		}

		if ( SDL_MUSTLOCK(display) ) {
			if ( SDL_LockSurface(display) < 0 ) {
				error();
			}
		}

		trace_update();

		{
			SDL_Color white = { 255, 255, 255 };
			SDL_Color black = { 0, 0, 0 };
			char buffer[100];
			SDL_Surface *txt;
			SDL_Rect dest = { 0, 0 };
			int seconds;
			int pixels_per_second;

			if (pixels_done < width*height)
				end_time = clock();
			seconds = (end_time - start_time) / CLOCKS_PER_SEC;
			pixels_per_second = (seconds > 0) ? pixels_done/seconds : 0;

			snprintf(buffer, sizeof(buffer), "done=%d/%d, PPS=%d, cx,cy=%f,%f, scale=%f, status=%s     ", pixels_done, width*height, pixels_per_second, centrex, centrey, scale, status);
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

	return 0;
}
