QJSPATH=../../quickjs-2025-09-13
#QJSPATH=../../quickjs-2023-12-09
#QJSPATH=../../quickjs-2021-03-27

CC=gcc
CFLAGS=-I$(QJSPATH) -fPIC
LDFLAGS=$(QJSPATH)/libquickjs.a

net.so: quickjs-net.o
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o net.so quickjs-net.o

quickjs-net.o: quickjs-net.c
	$(CC) $(CFLAGS) -c -o quickjs-net.o quickjs-net.c -D_GNU_SOURCE=1

clean:
	rm net.so quickjs-net.o

all: net.so
