CC = gcc
CFLAGS = -Wall -Wextra -O2 -Wno-deprecated-declarations -Wno-unused-result
LIBS = -lcrypto

SRCS = huffFork.c huffForkComp.c huffForkDecomp.c huffTreeFork.c formato.c
TARGET = huffFork

all: $(TARGET)

$(TARGET): $(SRCS) huffman.h formato.h
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean