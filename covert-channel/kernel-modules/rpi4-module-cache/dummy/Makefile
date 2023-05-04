CC=gcc
CFLAGS=-Wall
OBJ = dummy.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

dummy: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
