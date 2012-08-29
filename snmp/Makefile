CFLAGS = -g -Wall

OBJ=$(patsubst %.c, %.o, $(wildcard *.c))
SENDER_OBJ=sender.o snmp.o asn1.o
RECEIVER_OBJ=receiver.o snmp.o asn1.o
HEADERS=snmp.h asn1.h

all: sender receiver

clean:
	rm -f $(OBJ)

sender: $(SENDER_OBJ)
	$(CC) -o $@ $(SENDER_OBJ) $(CFLAGS)

receiver: $(RECEIVER_OBJ)
	$(CC) -o $@ $(RECEIVER_OBJ) $(CFLAGS)

%.o: %.c $(HEADERS)
	$(CC) -c $< $(CFLAGS)
