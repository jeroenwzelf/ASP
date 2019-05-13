#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

// An ASP packet (defined in Specification/ASP.txt)
typedef struct __attribute__((__packed__)) {
	uint16_t SOURCE_PORT;
	uint16_t DESTINATION_PORT;

	uint8_t FLAGS;
	uint8_t SEQ_NUMBER;

	uint16_t PAYLOAD_LENGTH;
	uint16_t CHECKSUM;
	void* data;
} asp_packet;

static uint8_t ASP_PACKET_HEADER_SIZE =
		  sizeof(uint16_t)	// SOURCE_PORT
		+ sizeof(uint16_t)	// DESTINATION_PORT
		+ sizeof(uint8_t)	// FLAGS
		+ sizeof(uint8_t)	// SEQ_NUMBER
		+ sizeof(uint16_t)	// PAYLOAD_LENGTH
		+ sizeof(uint16_t);	// CHECKSUM

// FLAGS
enum {
	ACK = 1 << 0,				// for acknowledgements
	REJ = 1 << 1,				// for rejection (client misses packets)
	NEW_CLIENT = 1 << 2,		// for a new connection
	DATA_WAV_HEADER = 1 << 3,	// for sending wav header
	DATA_WAV_SAMPLES = 1 << 4,	// for sending wav samples
	NEXT_EVENT = 1 << 5			// for start/end stream events
};

// ASP packet initialization
asp_packet create_asp_packet(uint16_t source, uint16_t dest, uint8_t flags, void* data, uint16_t data_size);
bool is_flag_set(asp_packet* packet, uint8_t flag);
uint16_t size(asp_packet* packet);

// Serialization for transferring packet over a socket
void* serialize_asp(asp_packet* packet);
asp_packet* deserialize_asp(void* buffer);