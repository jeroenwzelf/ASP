#include "asp_packet.h"

/*
// Prints a uint16_t in binary to std:out
void print(uint16_t n) {
	uint16_t c = n;
	for (int i = 0; i < 16; i++) {
		printf("%d", (n & 0x8000) >> 15);
		n <<= 1;
	}
	printf("\t(%i)\n", c);
}
*/

uint16_t ones_complement_sum(asp_packet* packet) {
	uint16_t* pseudoheader = packet;
	uint16_t* payload = packet->data;

	/* From RFC 1071: Adjacent octets are paired to form 16-bit
	integers, and the 1's complement sum of these 16-bit integers is
	formed. */
	uint16_t sum = 0;
	for (uint8_t i=0; i<4; ++i) sum += pseudoheader[i];
	for (uint16_t i=0; i<packet->PAYLOAD_LENGTH; ++i) sum += payload[i];

	return sum;
}

bool has_valid_checksum(asp_packet* packet) {
	/* From RFC 1071: To check a checksum, the 1's complement sum is computed over the
	same set of octets, including the checksum field.  If the result
	is all 1 bits (-0 in 1's complement arithmetic), the check
	succeeds. */

	// Note: all 1 bits is -1 in 2's complement arithmetic
	return true;
	return ( (int16_t) ones_complement_sum(packet) == -1);
}

asp_packet create_asp_packet(uint16_t source, uint16_t dest, void* data, uint16_t data_size) {
	asp_packet packet;

	packet.SOURCE_PORT = source;
	packet.DESTINATION_PORT = dest;
	packet.PAYLOAD_LENGTH = data_size;
	packet.data = data;

	/* To generate a checksum, the checksum field itself is cleared,
	the 16-bit 1's complement sum is computed over the octets
	concerned, and the 1's complement of this sum is placed in the
	checksum field. */
	packet.CHECKSUM = 0;
	packet.CHECKSUM = ~ones_complement_sum(&packet);

	return packet;
}

uint16_t size(asp_packet* packet) {
	return (4 * sizeof(uint16_t)) + packet->PAYLOAD_LENGTH;
}

// Copies the packet and its contents (actual data instead of pointer to data) into a buffer
void* serialize_asp(asp_packet* packet) {
	void* buffer = malloc(size(packet));

	// Convert header from host to network
	uint16_t htons_SOURCE_PORT = htons(packet->SOURCE_PORT);
	uint16_t htons_DESTINATION_PORT = htons(packet->DESTINATION_PORT);
	uint16_t htons_PAYLOAD_LENGTH = htons(packet->PAYLOAD_LENGTH);
	uint16_t htons_CHECKSUM = htons(packet->CHECKSUM);

	// Write struct to buffer
	memcpy(buffer + (0 * sizeof(uint16_t)), &htons_SOURCE_PORT, sizeof(uint16_t));
	memcpy(buffer + (1 * sizeof(uint16_t)), &htons_DESTINATION_PORT, sizeof(uint16_t));
	memcpy(buffer + (2 * sizeof(uint16_t)), &htons_PAYLOAD_LENGTH, sizeof(uint16_t));
	memcpy(buffer + (3 * sizeof(uint16_t)), &htons_CHECKSUM, sizeof(uint16_t));
	memcpy(buffer + (4 * sizeof(uint16_t)), packet->data, packet->PAYLOAD_LENGTH);

	return buffer;
}

// Converts a buffer into a valid ASP packet if it is one. If it was not a valid packet, it returns NULL
asp_packet* deserialize_asp(void* buffer) {
	asp_packet* packet = buffer;

	packet->SOURCE_PORT = ntohs(packet->SOURCE_PORT);
	packet->DESTINATION_PORT = ntohs(packet->DESTINATION_PORT);
	packet->PAYLOAD_LENGTH = ntohs(packet->PAYLOAD_LENGTH);
	packet->CHECKSUM = ntohs(packet->CHECKSUM);

	// Get payload from buffer
	void* payload = malloc(packet->PAYLOAD_LENGTH);
	memcpy(payload, buffer + (4 * sizeof(uint16_t)), packet->PAYLOAD_LENGTH);
	packet->data = payload;

	if (!has_valid_checksum(packet)) return NULL;
	return packet;
}