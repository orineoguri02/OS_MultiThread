# HW#3 — 과제 제출 바이너리 이름: mtws

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -pthread
LDFLAGS = -pthread

.PHONY: all clean

all: mtws

mtws: mtws.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f mtws
