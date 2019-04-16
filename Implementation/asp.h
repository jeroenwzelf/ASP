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

#include <errno.h>

typedef struct __attribute__((__packed__)) {
	uint16_t SOURCE_PORT;
	uint16_t DESTINATION_PORT;
	uint16_t PAYLOAD_LENGTH;
	uint16_t CHECKSUM;
	void* data;
} asp_packet;

/* An asp socket descriptor for information about the sockets current state */
struct asp_socket_info {
	int sockfd;

	struct sockaddr_in local_addr;
	socklen_t local_addrlen;

	struct sockaddr_in remote_addr;
	socklen_t remote_addrlen;

	struct asp_socket_info *next;

	int current_quality_level;
	int sequence_count;

	int packets_received;
	int packets_missing;

	struct timeval last_packet_received;
	struct timeval last_problem;

	unsigned int is_connected : 1;
	unsigned int has_accepted : 1;
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

void invalidate_socket(asp_socket * sock, asp_socket_state new_state, char* errorno);


void listen_asp_socket_server(int port, int buffer_length);
void write_asp_socket_client(char* ip, int port, int buffer_length);

uint16_t get_checksum(void* data);

asp_packet create_asp_packet(uint16_t source, uint16_t dest, void* data);