#include <string.h>

#include "pq.h"


static int width, height;
static PQ *pq;
static int *done;
static enum { SEEDING, TRACING, EDGING, FILLED } state;

extern void trace_restart(void);

void trace_init(int w, int h)
{
	width = w;
	height = h;
	pq = NULL;
	done = malloc(sizeof(int) * width * height);
	trace_restart();
}

typedef struct COORDS
{
	int x;
	int y;
} COORDS;


#define NUM_SEEDS 1000


void trace_restart(void)
{
	int i;
	if (pq)
		pq_destroy(pq);
	pq = pq_create(sizeof (COORDS), width*height*2);
	for (i = 0; i < NUM_SEEDS; i++)
	{
		COORDS c;
		c.x = rand() % width;
		c.y = rand() % height;
		pq_push(pq, -10, &c);
	}

	memset(done, 0, sizeof(int)*width*height);
	state = SEEDING;
}


extern int max;
extern int do_pixel(int x, int y);
extern int set_pixel(int x, int y, int k);
extern char *status;


static int push_edges(void)
{
	int i;

	for (i = 0; i < width; i++)
	{
		COORDS c2;

		if (!done[i])
		{
			c2.x = i;
			c2.y = 0;
			pq_push(pq, -10, &c2);
		}

		if (!done[(height-1)*width + i])
		{
			c2.x = i;
			c2.y = height-1;
			pq_push(pq, -10, &c2);
		}
	}

	for (i = 0; i < height; i++)
	{
		COORDS c2;

		if (!done[i*width])
		{
			c2.x = 0;
			c2.y = i;
			pq_push(pq, -10, &c2);
		}

		if (!done[i*width + width - 1])
		{
			c2.x = width - 1;
			c2.y = i;
			pq_push(pq, -10, &c2);
		}
	}

	state = EDGING;
}


static void fill_black(void)
{
	int i, j;

	for (i = 0; i < height; i++)
		for (j = 0; j < width; j++)
			if (!done[i*width + j])
			{
				set_pixel(j, i, max ? 256*256 : 64);
				done[i*width + j] = 1;
			}
	state = FILLED;
}


void trace_update(void)
{
	int quota = 250000;

	while (quota > 0)
	{
		COORDS c;
		int priority;
		static int dx[] = { -1, -1, -1, 0, 1, 1, 1, 0 };
		static int dy[] = { -1, 0, 1, 1, 1, 0, -1, -1 };
		int i;
		int val;

		if (pq->num_items <= 0)
			break;

		pq_pop(pq, &priority, &c);
		if (done[c.y*width + c.x])
			continue;
		
		if (priority == 0)
		{
			if (state == TRACING || state == SEEDING)
				push_edges();
			else if (state == EDGING)
				fill_black();
		}
		else if (state == SEEDING)
			state = TRACING;

		val = do_pixel(c.x, c.y);
		done[c.y*width + c.x] = 1;

		quota -= (val + 10);

		for (i = 0; i < 8; i++)
		{
			COORDS c2;
			c2.x = c.x + dx[i];
			c2.y = c.y + dy[i];
			if (c2.x < 0 || c2.y < 0 || c2.x >= width || c2.y >= height)
				continue;
			pq_push(pq, (val == (max ? 256*256 : 64)) ? 0 : -10-val-((c2.x ^ c2.y ^ quota) & 15), &c2);
		}
	}

	if (state == SEEDING)
		status = "SEEDING";
	else if (state == TRACING)
		status = "TRACING";
	else if (state == EDGING)
		status = "EDGING";
	else if (state == FILLED)
		status = "FILLED";
	else
		status = "UNKNOWN";
}
