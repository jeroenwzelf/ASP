/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "asp_packet.h"

#include <errno.h>

#define ASP_SERVER_PORT 1235
#define MAX_PACKET_SIZE 1024

/* An ASP socket descriptor for information about the sockets current state */
typedef struct {
	int sockfd;

	struct sockaddr_in local_addr;
	struct sockaddr_in remote_addr;

	int current_quality_level;
	int sequence_count;

	int packets_received;
	int packets_missing;

	struct timeval last_packet_received;
	struct timeval last_problem;

	bool is_connected;
	bool has_accepted;
} asp_socket_info;

// Socket state error handling
typedef enum {
	CREATE_SOCKET_FAILED,
	BIND_SOCKET_FAILED,
	SOCKET_WRITE_FAILED,
	SOCKET_READ_FAILED,
	SOCKET_INVALID_CONFIGURATION,
	WORKING
} asp_socket_state;

typedef struct
{
	asp_socket_state state;
	asp_socket_info info;
} asp_socket;

// Socket initialization
asp_socket new_socket(int local_PORT);
void set_remote_addr(asp_socket* sock, char* ip, int port);

// Socket data transfer
void send_packet(asp_socket* sock, void* packet, uint16_t packet_size);
void* receive_packet(asp_socket* sock);