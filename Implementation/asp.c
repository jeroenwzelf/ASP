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

void send_packet(asp_socket* sock, void* packet, uint16_t packet_size) {
	if (sock->state == WORKING) {
		// Print packet in HEX
		printf("packet in HEX:\n");
		char* packet_char = packet;
		for (int i=0; i<packet_size; ++i) {
			printf("%x ", packet_char[i] & 0XFF);
		}
		printf("\n");
		
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

		printf("packet in HEX:\n");
		char* packet_char = buf;
		uint16_t zero_count = 0;
		for (int i=0; i<MAX_PACKET_SIZE && zero_count < 5; ++i) {
			printf("%x ", packet_char[i] & 0XFF);
			if (packet_char[i] == 0) zero_count++;
		}
		printf("\n");
	}
	else fprintf(stderr, "Couldn't listen to socket: socket is invalid!\n");

	return buf;
}