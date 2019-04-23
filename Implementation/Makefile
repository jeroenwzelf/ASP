#
# Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
# Universiteit Leiden.
#

all:	server client

%.o:	%.c
	gcc -w -s -std=gnu99 -c $<

server:	wav_header.o server.o asp_packet.o asp.o
	gcc -Wall -s -o server wav_header.o server.o asp_packet.o asp.o -lm

client:	client.o wav_header.o asp_packet.o asp.o
	gcc -Wall -s -o client wav_header.o client.o asp_packet.o asp.o -lm -lasound

clean:
	rm -f server
	rm -f client
	rm -f *.o
