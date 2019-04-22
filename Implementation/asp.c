#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <netinet/in.h>

#include "asp.h"

// socket (error) state to string
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
		case SOCKET_INVALID_CONFIGURATION:
			return "SOCKET_INVALID_CONFIGURATION";
		case WORKING:
			return "WORKING";
	}
	return "UNDEFINED";
}

// Create a new socket and binds it to local_PORT.
// To listen to the socket, use receive_packet(sock)
// To send data over the socket to a remote address, configure the remote address
// 		with set_remote_addr(sock, ip, port)
//		and then send a packet with send_packet(sock, packet, packet_size)
asp_socket new_socket(int local_PORT) {
	printf("Creating socket on 127.0.0.1:%i\n", local_PORT);
	asp_socket sock;
	sock.state = WORKING;

	// Create new socket
	if ((sock.info.sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		invalidate_socket(&sock, CREATE_SOCKET_FAILED, strerror(errno));
		return sock;
	}

	// Set local address (0.0.0.0:local_PORT)
	memset((char*)&sock.info.local_addr, 0, sizeof(sock.info.local_addr));
	sock.info.local_addr.sin_family = AF_INET;
	sock.info.local_addr.sin_port = htons(local_PORT);
	sock.info.local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Initialize socket info
	sock.info.packets_received = 0;
	sock.info.packets_missing = 0;

	// Bind socket to port
	if (bind(sock.info.sockfd, &sock.info.local_addr, sizeof(sock.info.local_addr)) == -1) {
		invalidate_socket(&sock, BIND_SOCKET_FAILED, strerror(errno));
		return sock;
	}

	return sock;
}

// Sets the remote address (ip:port) for the socket
void set_remote_addr(asp_socket* sock, char* ip, int port) {
	// Set destination address (ip:port)
	memset((char*)&sock->info.remote_addr, ip, sizeof(sock->info.remote_addr));
	sock->info.remote_addr.sin_family = AF_INET;
	sock->info.remote_addr.sin_port = htons(port);

	// Check if remote IP address is correct
	if (inet_aton(ip, &sock->info.remote_addr.sin_addr) == 0) {
		invalidate_socket(&sock, SOCKET_INVALID_CONFIGURATION, strerror(errno));
	}
}

// Socket state error handling
void invalidate_socket(asp_socket* sock, asp_socket_state new_state, char* error) {
	fprintf(stderr, "Socket state changed from %s to %s (%s).\n", asp_socket_state_to_char(sock->state), asp_socket_state_to_char(new_state), error);
	sock->state = new_state;
}

// Prints v in hex, and cuts off irrelevant traling zero's
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

// Sends a packet of any form over the socket
void send_packet(asp_socket* sock, void* packet, uint16_t packet_size) {
	if (sock->state == WORKING) {
		printf("packet in hex:\n");
		print_hexvalues(packet, packet_size);
		
		if (sendto(sock->info.sockfd, packet, packet_size, 0, &sock->info.remote_addr, sizeof(sock->info.remote_addr)) == -1)
			invalidate_socket(sock, SOCKET_WRITE_FAILED, strerror(errno));
		else printf("Sent packet!\n\n");
	}
	else fprintf(stderr, "Couldn't send packet: socket is invalid!\n");
}

// Blocks until it receives a packet of any form over the socket
void* receive_packet(asp_socket* sock) {
	// Create empty buffer
	void* buf = calloc(MAX_PACKET_SIZE, sizeof(char));

	if (sock->state == WORKING) {
		// Wait for an incoming packet
		socklen_t remote_addrlen = sizeof(sock->info.remote_addr);
		if (recvfrom(sock->info.sockfd, buf, MAX_PACKET_SIZE, 0, &sock->info.remote_addr, &remote_addrlen) == -1)
			invalidate_socket(sock, SOCKET_READ_FAILED, strerror(errno));

		// Received a packet
		printf("Received packet from %s:%d\n",
			inet_ntoa(sock->info.remote_addr.sin_addr), ntohs(sock->info.remote_addr.sin_port));
		++sock->info.packets_received;

		printf("packet in hex:\n");
		print_hexvalues(buf, MAX_PACKET_SIZE);
	}
	else fprintf(stderr, "Couldn't listen to socket: socket is invalid!\n");

	return buf;
}