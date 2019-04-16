#
# Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
# Universiteit Leiden.
#

all:	server client

%.o:	%.c
	gcc -Wall -s -std=gnu99 -c $<

server:	server.o asp.o
	gcc -Wall -s -o server server.o asp.o -lm

client:	client.o asp.o
	gcc -Wall -s -o client client.o asp.o -lm -lasound

clean:
	rm -f server
	rm -f client
	rm -f *.o
