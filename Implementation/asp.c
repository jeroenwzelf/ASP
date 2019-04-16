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

asp_packet create_asp_packet(uint16_t source, uint16_t dest, void* data) {
	asp_packet packet;

	packet.SOURCE_PORT = source;
	packet.DESTINATION_PORT = dest;

	// parse data
	packet.data = data;

	// get length of data
	packet.PAYLOAD_LENGTH = sizeof(data);

	// calculate checksum
	packet.CHECKSUM = get_checksum(data);


	printf("packet->SOURCE_PORT: %i\n", packet.SOURCE_PORT);
	printf("packet->DESTINATION_PORT: %i\n", packet.DESTINATION_PORT);
	printf("packet->PAYLOAD_LENGTH: %i\n", packet.PAYLOAD_LENGTH);
	printf("packet->CHECKSUM: %i\n", packet.CHECKSUM);
	printf("packet->data: %s\n", (char*)packet.data);

	return packet;
}

uint16_t get_checksum(void* data) {
	//uint8_t* octets = data;
	//char* message = data;

	/*printf("HEX for '%s':\n", message);

	// Split data in octets
	for (int i=0; i<sizeof(octets); ++i) {
		printf("%x ", octets[i] & 0XFF);
	}
	printf("\n");*/


	return 5;

	/* Adjacent octets to be checksummed are paired to form 16-bit
	integers, and the 1's complement sum of these 16-bit integers is
	formed. */



	/* To generate a checksum, the checksum field itself is cleared,
	the 16-bit 1's complement sum is computed over the octets
	concerned, and the 1's complement of this sum is placed in the
	checksum field. */
}

char* parse_asp_packet(asp_packet * packet) {
	printf("Parsing packet...\n");
	printf("packet->SOURCE_PORT: %i\n", packet->SOURCE_PORT);
	printf("packet->DESTINATION_PORT: %i\n", packet->DESTINATION_PORT);
	printf("packet->PAYLOAD_LENGTH: %i\n", packet->PAYLOAD_LENGTH);
	printf("packet->CHECKSUM: %i\n", packet->CHECKSUM);
	printf("packet->data: %s\n", (char*)packet->data);

	return packet->data;
}

void invalidate_socket(asp_socket * sock, asp_socket_state new_state, char* error) {
	fprintf(stderr, "Socket state changed from %s to %s (%s).\n", asp_socket_state_to_char(sock->state), asp_socket_state_to_char(new_state), error);
	sock->state = new_state;
}