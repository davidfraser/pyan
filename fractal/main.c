#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include <SDL.h>
#include <SDL_ttf.h>


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

int mfunc(double cx, double cy, int max, double *fx, double *fy)
{
	int i = 0;
	double zr = 0.0, zi = 0.0;

	while (i < max && zr*zr + zi*zi < 2.0*2.0)
	{
		double t = zr;
		zr = zr*zr - zi*zi + cx;
		zi = 2*t*zi + cy;
		i++;
	}
	*fx = zr;
	*fy = zi;

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


static double centrex = 0.0, centrey = 0.0;
static double scale = 0.00125;
static int screen_width = 1920;
static int screen_height = 1080;
static int width = 1920*2;
static int height = 1080*2;
int max = 0;
static int pixels_done;
static SDL_Surface *display;
static int *buffer;
char *status = "?";


void set_pixel(int x, int y, int k)
{
	SDL_Color col;
	int x1 = x/2;
	int y1 = y/2;

	buffer[y*width + x] = k;
	k = buffer[(y1*2)*width + (x1*2)] + buffer[(y1*2)*width + (x1*2+1)] + buffer[(y1*2+1)*width + (x1*2)] + buffer[(y1*2+1)*width + (x1*2+1)];
	k /= 4;

	/*double px = (x - width/2.0)*scale + centrex;
	double py = (y - height/2.0)*scale + centrey;
	double fx, fy;
	double theta;*/
	if (k >= (max ? 256*16 : 64))
	{
		col.r = 0;
		col.g = 0;
		col.b = 0;
	}
	else
	{
		/*double f = sqrt(k)*4;
		double s;
		if (!max || f < 32)
			f = f*8;
		theta = atan2(fx, fy) + M_PI;
		if (f >= 128.0)
			s = (256.0 - f)/128.0;
		else
			s = f/128.0;
		hsl_to_colour(theta/M_PI/2.0, s, f/256.0, &col);
		*/
		double f = sqrt(k) / (double) (max ? 16*16 : 8);
		hsl_to_colour(0, 0, f, &col);
		col.r = k % 256;
		col.g = 255;
	}

	DrawPixel(display, col.r, col.g, col.b, x1, y1);
	pixels_done++;
}

int do_pixel(int x, int y)
{
	double px = (x - width/2.0)*scale + centrex;
	double py = (y - height/2.0)*scale + centrey;
	double fx, fy;
	double theta;

	int k = mfunc(px, py, max ? 256*256 : 64, &fx, &fy);

	set_pixel(x, y, k);

	if (k != 1)
	{
		k = mfunc(px, py, max ? 256*256 : 64, &fx, &fy);
	}

	return k;
}

int main(int argc, char *argv[])
{
    SDL_Event evt;
	int running = 1;
	TTF_Font *font;

	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        error();
    }

	if (TTF_Init() < 0) {
		error();
	}

	font = TTF_OpenFont("c:/windows/fonts/arial.ttf", 16);
	if (!font)
		error();

	display = SDL_SetVideoMode(screen_width, screen_height, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN);
    if(display == NULL) {
        error();
    }

	buffer = (int *) malloc(sizeof(int) * width * height);
	memset(buffer, 0, sizeof(int) * width * height);

	trace_init(width, height);

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
			}
			else if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == 1)
			{
				centrex = (evt.button.x - screen_width/2.0)*scale*2 + centrex;
				centrey = (evt.button.y - screen_height/2.0)*scale*2 + centrey;
				scale = scale * M_SQRT1_2;
				trace_restart();
				pixels_done = 0;
			}
			else if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == 3)
			{
				centrex = (evt.button.x - screen_width/2.0)*scale*2 + centrex;
				centrey = (evt.button.y - screen_height/2.0)*scale*2 + centrey;
				scale = scale / M_SQRT1_2;
				trace_restart();
				pixels_done = 0;
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

			sprintf_s(buffer, sizeof(buffer), "done=%d/%d, cx,cy=%f,%f, scale=%f, status=%s    ", pixels_done, width*height, centrex, centrey, scale, status);
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
	}

	TTF_Quit();
		
	SDL_Quit();

	return 0;
}
