all:s.out c.out

s.out:server.o service.o
	g++ -o s.out server.o service.o

server.o:server.cpp service.h
	g++ -I. -c server.cpp

service.o:service.cpp service.h
	g++ -I. -c service.cpp

c.out:
	gcc client.c -o c.out

clean:
	rm s.out
	rm server.o
	rm service.o
	rm c.out
