CC=clang++-7
CFLAGS=-I. -std=c++11 -W -Wall -w -O3
DEPS = SHA3Worker.h NIOReader.h SHA3.h 

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

sha3Test:  SHA3Worker.o testSHA3.o 
	$(CC) -lpthread  -o sha3Test  SHA3Worker.o testSHA3.o 

clean:
	rm *.o
