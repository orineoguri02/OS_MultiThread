# HW#3 — 과제 제출 바이너리 이름: mtws

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -pthread -Iinclude
LDFLAGS = -pthread

SRCS = src/main.c src/args.c src/buffer.c src/dirwalk.c src/search.c src/worker.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: mtws

mtws: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) mtws mtws_study
