CC=gcc
CFLAGS=-g -Wall -Wextra -pedantic -std=gnu99 -pthread
EXAMPLES=fibs fauxgrep fauxgrep-mt fhistogram fhistogram-mt
TESTS=

.PHONY: all test clean ../src.zip

all: $(TESTS) $(EXAMPLES)

job_queue.o: job_queue.c job_queue.h
	$(CC) -c job_queue.c $(CFLAGS)

%: %.c job_queue.o
	$(CC) -o $@ $^ $(CFLAGS)

fauxgrep: fauxgrep.c job_queue.o
	$(CC) $(CFLAGS) fauxgrep.c job_queue.o -o fauxgrep

fauxgrep-mt: fauxgrep-mt.c job_queue.o
	$(CC) $(CFLAGS) fauxgrep-mt.c job_queue.o -o fauxgrep-mt

fhistogram: fhistogram.c
	$(CC) $(CFLAGS) fhistogram.c -o fhistogram

fhistogram-mt: fhistogram-mt.c job_queue.o
	$(CC) $(CFLAGS) fhistogram-mt.c job_queue.o -o fhistogram-mt


test: $(TESTS)
	@set e; for test in $(TESTS); do echo ./$$test; ./$$test; done

clean:
	rm -rf $(TESTS) $(EXAMPLES) *.o core

zip: ../src.zip

../src.zip:
	make clean
	cd .. && zip src.zip -r src
