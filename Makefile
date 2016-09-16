CC=clang
CFLAGS=-Wall -g

BINS= notjustcats


all: $(BINS)

notjustcats:  cats.c 
	$(CC) $(CFLAGS) -o notjustcats cats.c

clean:
	rm $(BINS)

