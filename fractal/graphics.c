#include "graphics.h"

#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>


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

void error(void)
{
    fprintf(stderr, "SDL error: %s\n", SDL_GetError());
        exit(1);
}
