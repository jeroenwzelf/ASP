#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct __attribute__((__packed__)) {
	uint16_t SOURCE_PORT;
	uint16_t DESTINATION_PORT;
	uint16_t PAYLOAD_LENGTH;
	uint16_t CHECKSUM;
	void* data;
} asp_packet;

// ASP packet initialization
asp_packet create_asp_packet(uint16_t source, uint16_t dest, void* data, uint16_t data_size);
uint16_t size(asp_packet* packet);

// Serialization for transferring packet over a socket
void* serialize_asp(asp_packet* packet);
asp_packet* deserialize_asp(void* message);