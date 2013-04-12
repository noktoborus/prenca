# vim: ft=make ff=unix fenc=utf-8
# file: Makefile
LIBS=`pkg-config --cflags --libs enca`
CFLAGS=-g -Wall -pedantic -std=c99
BIN=./renca
SRC=renca.c opts.c

all: ${BIN}
	${BIN}

${BIN}: ${SRC}
	${CC} -o ${BIN} ${CFLAGS} ${SRC} ${LIBS}

