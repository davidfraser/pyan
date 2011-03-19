CFLAGS=-Dtest_main=main -Wall -g -pg
LDFLAGS=-g -pg

FS_OBJ=$(patsubst %.c, %.o, $(wildcard *.c))


all: $(FS_OBJ)

clean:
	rm -f $(FS_OBJ)
