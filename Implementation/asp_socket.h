#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "asp_packet.h"
#include "wav_header.h"

#include <errno.h>

#define ASP_SERVER_PORT	1235
#define MAX_PACKET_SIZE	524296
#define ASP_PACKET_WAV_SAMPLES 64
#define ASP_WINDOW		20

/* An ASP socket descriptor for information about the sockets current state */
typedef struct {
	int sockfd;

	struct sockaddr_in local_addr;
	struct sockaddr_in remote_addr;

	uint8_t current_quality_level;
	uint16_t sequence_count;

	int packets_received;
	int packets_missing;

	struct timeval last_packet_received;
	struct timeval last_problem;
} asp_socket_info;

typedef struct
{
	uint8_t* current_sample;
	uint32_t samples_done;
	uint32_t sample_size;
	uint32_t client_buffer_size;
} asp_socket_stream;

// Socket state error handling
typedef enum {
	CREATE_SOCKET_FAILED,
	BIND_SOCKET_FAILED,
	SOCKET_WRITE_FAILED,
	SOCKET_READ_FAILED,
	SOCKET_WRONG_REMOTE_ADDRESS,
	CHANGING_SOCKET_STATE_FAILED,
	WORKING
} asp_socket_state;

typedef struct
{
	asp_socket_state state;
	asp_socket_info info;
	asp_socket_stream* stream;
} asp_socket;

typedef struct socket_list
{
	asp_socket* sock;
	struct socket_list* next;
} asp_socket_list;

// Socket initialization
asp_socket new_socket(const int local_PORT);
void destroy_socket(asp_socket* sock);
void set_remote_addr(asp_socket* sock, const char* ip, const int port);
void set_socket_timeout(asp_socket* sock, time_t seconds, suseconds_t microseconds);
char* socket_address_string(struct sockaddr_in address);

// Socket data transfer
// void send_packet(asp_socket* sock, void* packet, uint16_t packet_size);
void* receive_packet(asp_socket* sock, const int flags);

// Specific packet sends
void asp_send_event(asp_socket* sock, const uint16_t flags);
void asp_send_rejection(asp_socket* sock, const uint16_t last_packet_sequence_number);
void asp_send_client_info(asp_socket* sock, const uint32_t buffer_size);
void asp_send_wav_header(asp_socket* sock, const struct wave_header* wh);
void asp_send_wav_samples(asp_socket* sock, const uint8_t* samples, const uint16_t amount, const uint16_t packet_sequence_number);

// Audio quality
uint8_t max_downsampling_size(uint32_t buffer_size, uint32_t sample_size);