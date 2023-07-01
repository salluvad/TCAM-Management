CC=gcc
CFLAGS=-I.
DEPS = list_api.h queue.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

tcam: tcam_ex.o list_api.o queue.o
	$(CC) -o tcam -std=c99 tcam_ex.o list_api.o queue.o
