CC=gcc
CFLAGS=-Wall
DEPS = ../cache_ioarams.h ../cache_structures.h
OBJ = ioctl.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ioctl: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
