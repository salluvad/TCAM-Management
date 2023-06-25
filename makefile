CC=gcc
CFLAGS=-I.
DEPS = list_api.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

tcam: tcam_ex.o list_api.o 
	$(CC) -o tcam -std=c99 tcam_ex.o list_api.o 
