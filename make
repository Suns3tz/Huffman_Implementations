CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS = -lcrypto

SRCS = huffSerial.c HuffFrec.c HuffTree.c huffComp.c huffDecomp.c
OBJS = $(SRCS:.c=.o)
TARGET = huffSerial

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

