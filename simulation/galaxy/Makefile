CFLAGS = -g -Wall -O3

OBJ=$(patsubst %.c, %.o, $(wildcard *.c))
HEADERS=galaxy.h

all: galaxy

galaxy: $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) -lm -lpng

%.o: %.c $(HEADERS)
	$(CC) -c $< $(CFLAGS)

avi:
	avidemux2_cli --load *.png --output-format AVI --video-codec FFmpeg4 --save test.avi --quit
