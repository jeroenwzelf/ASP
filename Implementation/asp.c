#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <netinet/in.h>

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
	return "UNDEFINED";
}

void invalidate_socket(asp_socket* sock, asp_socket_state new_state, char* error) {
	fprintf(stderr, "Socket state changed from %s to %s (%s).\n", asp_socket_state_to_char(sock->state), asp_socket_state_to_char(new_state), error);
	sock->state = new_state;
}

void print_hexvalues(void* v, uint16_t size) {
	char* buff = v;
	uint16_t zero_count = 0;
	for (int i=0; i<size; ++i) {
		printf("%x ", buff[i] & 0XFF);
		if (buff[i] != 0) zero_count = 0;
		else if (++zero_count > 2) break;
	}
	printf("\n");
}

void send_packet(asp_socket* sock, void* packet, uint16_t packet_size) {
	if (sock->state == WORKING) {
		printf("packet in hex:\n");
		print_hexvalues(packet, packet_size);
		
		// Send packet
		if (sendto(sock->info.sockfd, packet, packet_size, 0, &sock->info.remote_addr, sock->info.remote_addrlen) == -1)
			invalidate_socket(sock, SOCKET_WRITE_FAILED, strerror(errno));
		printf("Sent packet!\n\n");
	}
	else fprintf(stderr, "Couldn't send packet: socket is invalid!\n");
}

void* receive_packet(asp_socket* sock) {
	// Create empty buffer
	void* buf = calloc(MAX_PACKET_SIZE, sizeof(char));

	if (sock->state == WORKING) {
		// Wait for an incoming packet
		if (recvfrom(sock->info.sockfd, buf, MAX_PACKET_SIZE, 0, &sock->info.remote_addr, &sock->info.remote_addrlen) == -1)
			invalidate_socket(sock, SOCKET_READ_FAILED, strerror(errno));

		// Received a packet
		printf("Received packet from %s:%d\n",
			inet_ntoa(sock->info.remote_addr.sin_addr), ntohs(sock->info.remote_addr.sin_port));

		printf("packet in hex:\n");
		print_hexvalues(buf, MAX_PACKET_SIZE);
	}
	else fprintf(stderr, "Couldn't listen to socket: socket is invalid!\n");

	return buf;
}