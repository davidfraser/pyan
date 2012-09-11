CFLAGS = -g -Wall

OBJ=$(patsubst %.c, %.o, $(wildcard *.c))
HEADERS=snmp.h asn1.h config.h

all: poller

clean:
	rm -f $(OBJ)

poller: $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS)

%.o: %.c $(HEADERS)
	$(CC) -c $< $(CFLAGS)
