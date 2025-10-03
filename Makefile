# Makefile

# プログラム名とオブジェクトファイル名
PROGRAM = gomoku
OBJS = gomoku.o gomoku_ai.o .libgomoku.o

CC = gcc
CFLAGS = -Wall -O3 

all:	$(PROGRAM)

$(PROGRAM):	$(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(PROGRAM)

.PHONY: clean
clean:
	$(RM) $(PROGRAM) $(OBJS)
