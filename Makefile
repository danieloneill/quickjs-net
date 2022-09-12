QJSPATH=../../quickjs-2021-03-27

CC=gcc
CFLAGS=-I$(QJSPATH)
LDFLAGS=$(QJSPATH)/libquickjs.a

net.so: quickjs-net.o
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o net.so quickjs-net.o

quickjs-net.o: quickjs-net.c
	$(CC) $(CFLAGS) -c -o quickjs-net.o quickjs-net.c

clean:
	rm net.so quickjs-net.o

all: net.so
