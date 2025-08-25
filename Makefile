CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99

LDFLAGS = -lncurses

EXEC = myrandr

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

.DEFAULT_GOAL := all

.PHONY: all clean run

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC)

run: all
	./$(EXEC)

tui.o: xrandr_parser.h
xrandr_parser.o: xrandr_parser.h
