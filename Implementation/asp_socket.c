#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "asp_socket.h"

// socket (error) state to string
char* asp_socket_state_to_char(const asp_socket_state s) {
	switch (s) {
		case CREATE_SOCKET_FAILED:
			return "CREATE_SOCKET_FAILED";
		case BIND_SOCKET_FAILED:
			return "BIND_SOCKET_FAILED";
		case SOCKET_READ_FAILED:
			return "SOCKET_READ_FAILED";
		case SOCKET_WRITE_FAILED:
			return "SOCKET_WRITE_FAILED";
		case SOCKET_WRONG_REMOTE_ADDRESS:
			return "SOCKET_WRONG_REMOTE_ADDRESS";
		case CHANGING_SOCKET_STATE_FAILED:
			return "CHANGING_SOCKET_STATE_FAILED";
		case WORKING:
			return "WORKING";
	}
	return "UNDEFINED";
}

// Socket state error handling
void set_socket_state(asp_socket* sock, const asp_socket_state new_state, const char* error) {
	if (sock == NULL) return;
	char* sock_addr = socket_address_string(sock->info.local_addr);
	uint32_t sock_port = ntohs(sock->info.local_addr.sin_port);
	fprintf(stderr, "%s:%u socket state changed from %s to %s (%s).\n", sock_addr, sock_port,
			asp_socket_state_to_char(sock->state), asp_socket_state_to_char(new_state), error);
	sock->state = new_state;
	free(sock_addr);
}

char* socket_address_string(struct sockaddr_in address) {
	char* ipaddress = malloc(INET_ADDRSTRLEN * sizeof(char));
	inet_ntop( AF_INET, &address.sin_addr, ipaddress, INET_ADDRSTRLEN );
	return ipaddress;
}

// Creates a new socket and binds it to local_PORT.
// To listen to the socket, use receive_packet(sock)
// To send data over the socket to a remote address, configure the remote address
// 		with set_remote_addr(sock, ip, port)
//		and then send a packet with send_packet(sock, packet, packet_size)
asp_socket new_socket(const int local_PORT) {
	asp_socket sock;
	sock.state = WORKING;

	// Create new socket
	if ((sock.info.sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		set_socket_state(&sock, CREATE_SOCKET_FAILED, strerror(errno));
		return sock;
	}

	// Set local address (0.0.0.0:local_PORT)
	memset((char*)&sock.info.local_addr, 0, sizeof(sock.info.local_addr));
	sock.info.local_addr.sin_family = AF_INET;
	sock.info.local_addr.sin_port = htons(local_PORT);
	sock.info.local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Initialize socket info
	sock.info.downsampling = 1;
	sock.info.current_quality_level = 5;
	sock.info.sequence_count = 0;
	sock.info.packets_received = 0;
	sock.info.packets_missing = 0;
	sock.stream = NULL;

	// Bind socket to port
	if (bind(sock.info.sockfd, &sock.info.local_addr, sizeof(sock.info.local_addr)) == -1) {
		set_socket_state(&sock, BIND_SOCKET_FAILED, strerror(errno));
		return sock;
	}

	// Print socket location
	socklen_t local_addr_len = sizeof(sock.info.local_addr);
	if (getsockname(sock.info.sockfd, &sock.info.local_addr, &local_addr_len) != 0)
		fprintf(stderr, "%s\n", strerror(errno));

	char* sock_addr = socket_address_string(sock.info.local_addr);
	uint32_t sock_port = ntohs(sock.info.local_addr.sin_port);
	printf("Creating socket on %s:%u\n", sock_addr, sock_port);
	free(sock_addr);

	return sock;
}

void set_socket_timeout(asp_socket* sock, time_t seconds, suseconds_t microseconds) {
	struct timeval read_timeout;
	read_timeout.tv_sec = seconds;
	read_timeout.tv_usec = microseconds;

	setsockopt(sock->info.sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));
}

// Sets the remote address (ip:port) for a socket
void set_remote_addr(asp_socket* sock, const char* ip, const int port) {
	if (sock == NULL) return;
	// Set destination address (ip:port)
	// memset((char*)&sock->info.remote_addr, ip, sizeof(sock->info.remote_addr));
	sock->info.remote_addr.sin_family = AF_INET;
	sock->info.remote_addr.sin_port = htons(port);

	// Check if remote IP address is correct
	if (inet_aton(ip, &sock->info.remote_addr.sin_addr) == 0)
		set_socket_state(sock, SOCKET_WRONG_REMOTE_ADDRESS, strerror(errno));
	else if (sock->state == SOCKET_WRONG_REMOTE_ADDRESS)
		set_socket_state(sock, WORKING, "remote IP is corrected");
}

void destroy_socket(asp_socket* sock) {
	if (sock == NULL) return;
	close(sock->info.sockfd);
	free(sock->stream);
}

// Sends a packet of any form over a socket
void send_packet(asp_socket* sock, void* packet, uint16_t packet_size) {
	if (sock == NULL) return;
	if (sock->state == WORKING) {
		if (sendto(sock->info.sockfd, packet, packet_size, 0, &sock->info.remote_addr, sizeof(sock->info.remote_addr)) == -1)
			set_socket_state(sock, SOCKET_WRITE_FAILED, strerror(errno));
	}
	else fprintf(stderr, "Couldn't send packet: socket is invalid!\n");
}

// Blocks until it receives a packet of any form over a socket
// Or, if the socket has a timeout, it will return NULL when it didn't get a packet after the timeout
void* receive_packet(asp_socket* sock, const int flags) {
	if (sock == NULL) return NULL;
	// Create empty buffer
	void* buf = calloc(MAX_PACKET_SIZE, sizeof(char));

	if (sock->state == WORKING) {
		// Wait for an incoming packet
		socklen_t remote_addrlen = sizeof(sock->info.remote_addr);
		if (recvfrom(sock->info.sockfd, buf, MAX_PACKET_SIZE, flags, &sock->info.remote_addr, &remote_addrlen) == -1) {
			if (errno != EAGAIN) set_socket_state(sock, SOCKET_READ_FAILED, strerror(errno));
		}
		else {
			// Received a packet
			++sock->info.packets_received;
			return buf;
		}
	}

	free(buf);
	return NULL;
}

void asp_send_event(asp_socket* sock, const uint16_t flags) {
	asp_packet packet = create_asp_packet(
				ntohs(sock->info.local_addr.sin_port),
				ntohs(sock->info.remote_addr.sin_port),
				flags, 0,
				NULL, 0);
	send_packet(sock, serialize_asp(&packet), size(&packet));
}

void asp_send_rejection(asp_socket* sock, const uint16_t last_packet_sequence_number) {
	asp_packet packet = create_asp_packet(
				ntohs(sock->info.local_addr.sin_port),
				ntohs(sock->info.remote_addr.sin_port),
				REJ, 0,
				&last_packet_sequence_number, 2);
	send_packet(sock, serialize_asp(&packet), size(&packet));
}

void asp_send_client_info(asp_socket* sock, const uint32_t buffer_size) {
	asp_packet packet = create_asp_packet(
				ntohs(sock->info.local_addr.sin_port),
				ntohs(sock->info.remote_addr.sin_port),
				NEW_CLIENT, 0,
				&buffer_size, 4);
	send_packet(sock, serialize_asp(&packet), size(&packet));
}

void asp_send_wav_header(asp_socket* sock, const struct wave_header* wh) {
	void* buffer = serialize_wav_header(wh);
	asp_packet packet = create_asp_packet(
				ntohs(sock->info.local_addr.sin_port),
				ntohs(sock->info.remote_addr.sin_port),
				DATA_WAV_HEADER, 0,
				buffer, WAV_HEADER_SIZE);
	send_packet(sock, serialize_asp(&packet), size(&packet));
	free(buffer);
}

void asp_send_wav_samples(asp_socket* sock, const uint8_t* samples, const uint16_t amount, const uint16_t packet_sequence_number) {
	asp_packet packet = create_asp_packet(
				ntohs(sock->info.local_addr.sin_port),
				ntohs(sock->info.remote_addr.sin_port),
				DATA_WAV_SAMPLES, packet_sequence_number,
				samples, amount);
	send_packet(sock, serialize_asp(&packet), size(&packet));
}

uint8_t get_downsampling_quality(uint8_t quality, uint32_t buffer_size, uint32_t sample_size) {
	// With downsampling, the amount of samples a client can play from one packet will get much larger.
	// As the client only has a limited buffer, a client should be able to extract samples from at least one packet.
	uint8_t max_downsampling = buffer_size / sample_size / ASP_PACKET_WAV_SAMPLES;
	switch (quality) {
		case 1: return max_downsampling;	// worst quality possible
		case 5: return 1;					// best quality possible
		default: {							// some quality in between the best and the worst
			uint8_t downsampling = (6 - quality) * (max_downsampling / 5);
			// round up to the nearest power of 2
			downsampling--;
			downsampling |= downsampling >> 1;
			downsampling |= downsampling >> 2;
			downsampling |= downsampling >> 4;
			downsampling |= downsampling >> 8;
			downsampling |= downsampling >> 16;
			downsampling++;
			return (downsampling > 1) ? downsampling : 1;
		}
	}
}