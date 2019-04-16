/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <netinet/in.h>

#include <math.h>

#include "asp.h"

char* asp_socket_state_to_char(asp_socket_state s) {
	switch (s) {
		case CREATE_SOCKET_FAILED:
			return "CREATE_SOCKET_FAILED";
		case BIND_SOCKET_FAILED:
			return "BIND_SOCKET_FAILED";
		case SOCKET_READ_FAILED:
			return "SOCKET_READ_FAILED";
		case SOCKET_WRITE_FAILED:
			return "SOCKET_WRITE_FAILED";
		case WORKING:
			return "WORKING";
	}
}

void listen_asp_socket_server(int port, int buffer_length) {
	asp_socket sock;

	sock.state = WORKING;

	// Create new socket
	if ((sock.info.sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		invalidate_socket(&sock, CREATE_SOCKET_FAILED, strerror(errno));
		return;
	}

	memset((char *) &sock.info.local_addr, 0, sizeof(sock.info.local_addr));
	sock.info.local_addr.sin_family = AF_INET;
	sock.info.local_addr.sin_port = htons(port);
	sock.info.local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock.info.sockfd, &sock.info.local_addr, sizeof(sock.info.local_addr)) == -1)
		invalidate_socket(&sock, BIND_SOCKET_FAILED, strerror(errno));


	int NPACK = 10;
	char buf[buffer_length];
	for (int i=0; i<NPACK; ++i) {

		sock.info.local_addrlen = sizeof(sock.info.local_addr);
		sock.info.remote_addrlen = sizeof(sock.info.remote_addr);

		if (recvfrom(sock.info.sockfd, buf, buffer_length, 0, &sock.info.remote_addr, &sock.info.remote_addrlen) == -1)
			invalidate_socket(&sock, SOCKET_READ_FAILED, strerror(errno));

		printf("Received packet from %s:%d\nData: %s\n\n",
			 inet_ntoa(sock.info.remote_addr.sin_addr), ntohs(sock.info.remote_addr.sin_port), buf);
	}
}

void write_asp_socket_client(char* ip, int port, int buffer_length) {
	asp_socket sock;
	sock.state = WORKING;

	// Create new socket
	if ((sock.info.sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		invalidate_socket(&sock, CREATE_SOCKET_FAILED, strerror(errno));
		return;
	}

	// Set destination address
	memset((char *) &sock.info.remote_addr, 0, sizeof(sock.info.remote_addr));
	sock.info.remote_addr.sin_family = AF_INET;
	sock.info.remote_addr.sin_port = htons(port);
	if (inet_aton(ip, &sock.info.remote_addr.sin_addr) == 0) {
		invalidate_socket(&sock, SOCKET_WRITE_FAILED, strerror(errno));
		return;
	}

	sock.info.local_addrlen = sizeof(sock.info.local_addr);
	sock.info.remote_addrlen = sizeof(sock.info.remote_addr);

	int NPACK = 10;
	for (int i=0; i < NPACK; ++i) {
		printf("Parsing packet %d\n", i);

		char packetbuf[buffer_length];
		sprintf(packetbuf, "This is packet %d", i);
		asp_packet packet = create_asp_packet(sock.info.local_addr.sin_port, sock.info.remote_addr.sin_port, packetbuf);


		printf("Sending packet %d\n", i);

		if (sendto(sock.info.sockfd, &packet, sizeof(packet), 0, &sock.info.remote_addr, sock.info.remote_addrlen) == -1) {
			invalidate_socket(&sock, SOCKET_WRITE_FAILED, strerror(errno));
			return;
		}
	}
}

asp_packet create_asp_packet(uint16_t source, uint16_t dest, void* data) {
	asp_packet packet;

	packet.SOURCE_PORT = source;
	packet.DESTINATION_PORT = dest;

	// parse data
	char* payload = data;

	// get length of data
	packet.PAYLOAD_LENGTH = sizeof(payload);

	// calculate checksum
	packet.CHECKSUM = get_checksum(data);

	return packet;
}

uint16_t get_checksum(void* data) {
	uint8_t* octets = data;
	char* message = data;

	printf("HEX for: '%s'\n", message);

	// Split data in octets
	bool s = true;
	for (int i=0; i<sizeof(octets); i+=1) {
		if (s) printf("%x ", octets[i] & 0XFF);
		else printf("%x\n", octets[i] & 0XFF);
		s = !s;
	}


	return 0;

	/* Adjacent octets to be checksummed are paired to form 16-bit
	integers, and the 1's complement sum of these 16-bit integers is
	formed. */



	/* To generate a checksum, the checksum field itself is cleared,
	the 16-bit 1's complement sum is computed over the octets
	concerned, and the 1's complement of this sum is placed in the
	checksum field. */
}

void invalidate_socket(asp_socket * sock, asp_socket_state new_state, char* errorno) {
	fprintf(stderr, "Socket state changed from %s to %s (%s).\n", asp_socket_state_to_char(sock->state), asp_socket_state_to_char(new_state), errorno);
	sock->state = new_state;
	exit(1);
}