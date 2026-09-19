CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LIBS = -lcrypto -pthread

SRCS = huffPthread.c HuffFrec_pthread.c HuffTree_pthread.c huffComp_pthread.c huffDecomp_pthread.c
OBJS = $(SRCS:.c=.o)
TARGET = huffPthread

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

%.o: %.c huffman_pthread.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

