#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

extern bool VERBOSE_LOGGING;

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

static const uint8_t ASP_PACKET_HEADER_SIZE =
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
asp_packet create_asp_packet(const uint16_t source, const uint16_t dest,
	const uint8_t flags, const uint8_t seq_number, const void* data, const uint16_t data_size);
bool is_flag_set(const asp_packet* packet, const uint8_t flag);
uint16_t size(const asp_packet* packet);

// Verbose ASP packet logging
void print_packet(const asp_packet* packet);
void print_flags(uint8_t flags);

// Serialization for transferring packet over a socket
void* serialize_asp(const asp_packet* packet);
asp_packet* deserialize_asp(const void* buffer);