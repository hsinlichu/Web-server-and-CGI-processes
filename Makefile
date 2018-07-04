all: server file_reader file_readerslow
server: server.o function.o
	gcc server.o function.o -o server
server.o: server.c 
	gcc -c server.c
function.o: function.c function.h
	gcc -c function.c
file_reader: file_reader.c
	gcc -o file_reader file_reader.c
file_readerslow: file_readerslow.c
	gcc -o file_readerslow file_readerslow.c

clean:
	rm *.o
	rm server file_reader file_readerslow
