CFLAGS=-D_FILE_OFFSET_BITS=64 -Wall -g -pg
LDFLAGS=-lfuse
TARGET=fs

FS_OBJ=$(patsubst %.c, %.o, $(wildcard ../*.c))

all: $(TARGET)

$(TARGET): main.o $(FS_OBJ)
	gcc -o $(TARGET) main.o $(FS_OBJ) $(LDFLAGS)
