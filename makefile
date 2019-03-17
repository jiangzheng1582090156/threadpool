.PHONY:clean veryclean run

CFLAGS=-Wall
LIB=-lpthread

test:test.o threadpool.o
	gcc -o $@ $^ $(CFLAGS) $(LIB)

.c.o:
	gcc -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o
veryclean:
	clean rm -f test
run:
	./test