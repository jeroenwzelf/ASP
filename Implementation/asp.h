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
#include <stdbool.h>

#include "asp_packet.h"

#include <errno.h>

#define ASP_SERVER_PORT 1235
#define MAX_PACKET_SIZE 1024

/* An ASP socket descriptor for information about the sockets current state */
struct asp_socket_info {
	int sockfd;

	struct sockaddr_in local_addr;
	socklen_t local_addrlen;

	struct sockaddr_in remote_addr;
	socklen_t remote_addrlen;

	int current_quality_level;
	int sequence_count;

	int packets_received;
	int packets_missing;

	struct timeval last_packet_received;
	struct timeval last_problem;

	bool is_connected;
	bool has_accepted;
};

typedef enum {
	CREATE_SOCKET_FAILED,
	BIND_SOCKET_FAILED,
	SOCKET_WRITE_FAILED,
	SOCKET_READ_FAILED,
	WORKING
} asp_socket_state;

typedef struct
{
	asp_socket_state state;
	struct asp_socket_info info;
} asp_socket;


void invalidate_socket(asp_socket* sock, asp_socket_state new_state, char* error);

void send_packet(asp_socket* sock, void* packet, uint16_t packet_size);
void* receive_packet(asp_socket* sock);