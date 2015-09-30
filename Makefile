#
# Sample Makefile for lsh
# lab1 in Operating System course
#
BIN=	lsh

SRCS=	parse.c lsh-1.c
OBJS=	parse.o lsh-1.o

CC=	clang
CFLAGS= -g 
CPPFLAGS= -I/usr/local/opt/readline/include
## Turn on this for more warnings:
#CFLAGS= -g -Wall -pedantic
LIBS= -lreadline -ltermcap

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

all:	$(BIN)

lsh:	$(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJS) $(LIBS)

clean:
	-rm -f $(OBJS) lsh
