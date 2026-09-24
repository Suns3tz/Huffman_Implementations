CC = gcc
CFLAGS = -Wall -Wextra -Wno-deprecated-declarations

all: huffSerial

huffSerial: huffComp.c huffDecomp.c HuffFrec.c HuffTree.c huffSerial.c
	$(CC) $(CFLAGS) $^ -o $@ -lcrypto -lm

clean:
	rm -f huffSerial
