CC = gcc
CFLAGS = -Wall -Wextra -O2
GTK_FLAGS = $(shell pkg-config --cflags --libs gtk+-3.0)

TARGET = gui_app
SRCS = gui_base.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(GTK_FLAGS)

clean:
	rm -f $(TARGET)
