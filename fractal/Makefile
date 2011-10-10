CFLAGS = -g -Wall $(OPT) -lSDL -lSDL_ttf -I /usr/include/SDL -lgomp -fopenmp

OBJ=$(patsubst %.c, %.o, $(wildcard *.c))
HEADERS=fractal.h

all: fractal

fractal: $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) -lm

%.o: %.c $(HEADERS)
	$(CC) -c $< $(CFLAGS)

clean:
	rm -f $(OBJ)
