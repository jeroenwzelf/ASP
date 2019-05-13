#include "asp_packet.h"

uint16_t ones_complement_sum(asp_packet* packet) {
	/* From RFC 1071: Adjacent octets are paired to form 8-bit
	integers, and the 1's complement sum of these 8-bit integers is
	formed. */
	uint8_t sum = 0;
	for (uint8_t i=0; i<10; ++i) sum += ((uint8_t*)packet)[i];	// header is 12 octets long (6 uint16_t), data is PAYLOAD_LENGTH long
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

asp_packet create_asp_packet(uint16_t source, uint16_t dest, uint8_t flags, void* data, uint16_t data_size) {
	asp_packet packet;

	packet.SOURCE_PORT = source;
	packet.DESTINATION_PORT = dest;

	packet.FLAGS = flags;
	packet.SEQ_NUMBER = 0;

	packet.PAYLOAD_LENGTH = data_size;
	packet.data = data;

	/* From RFC 1071: To generate a checksum, the checksum field itself is cleared,
	the 16-bit 1's complement sum is computed over the octets
	concerned, and the 1's complement of this sum is placed in the
	checksum field. */
	packet.CHECKSUM = 0;
	packet.CHECKSUM = ~ones_complement_sum(&packet);

	return packet;
}

bool is_flag_set(asp_packet* packet, uint8_t flag) {
	if (packet == NULL) return false;
	return ((packet->FLAGS & flag) == flag);
}

uint16_t size(asp_packet* packet) {
	if (packet == NULL) return 0;
	return (ASP_PACKET_HEADER_SIZE + (packet->PAYLOAD_LENGTH * sizeof(uint8_t)));
}

// Copies the packet and its contents (actual data instead of pointer to data) into a buffer
void* serialize_asp(asp_packet* packet) {
	if (packet == NULL) return NULL;
	void* buffer = malloc(size(packet));

	// Convert header from host to network
	uint16_t htons_SOURCE_PORT = htons(packet->SOURCE_PORT);
	uint16_t htons_DESTINATION_PORT = htons(packet->DESTINATION_PORT);
	uint16_t htons_PAYLOAD_LENGTH = htons(packet->PAYLOAD_LENGTH);
	uint8_t htons_FLAGS = packet->FLAGS;
	uint8_t htons_SEQ_NUMBER = packet->SEQ_NUMBER;
	uint16_t htons_CHECKSUM = htons(packet->CHECKSUM);

	// Write struct to buffer
	memcpy(buffer + (0 * sizeof(uint8_t)), &htons_SOURCE_PORT, sizeof(uint16_t));
	memcpy(buffer + (2 * sizeof(uint8_t)), &htons_DESTINATION_PORT, sizeof(uint16_t));
	memcpy(buffer + (4 * sizeof(uint8_t)), &htons_FLAGS, sizeof(uint8_t));
	memcpy(buffer + (5 * sizeof(uint8_t)), &htons_SEQ_NUMBER, sizeof(uint8_t));
	memcpy(buffer + (6 * sizeof(uint8_t)), &htons_PAYLOAD_LENGTH, sizeof(uint16_t));
	memcpy(buffer + (8 * sizeof(uint8_t)), &htons_CHECKSUM, sizeof(uint16_t));
	memcpy(buffer + (10 * sizeof(uint8_t)), packet->data, packet->PAYLOAD_LENGTH * sizeof(uint8_t));

	return buffer;
}

// Converts a buffer into a valid ASP packet if it is one. If it was not a valid packet, it returns NULL
asp_packet* deserialize_asp(void* buffer) {
	if (buffer == NULL) return NULL;
	asp_packet* packet = buffer;

	packet->SOURCE_PORT = ntohs(packet->SOURCE_PORT);
	packet->DESTINATION_PORT = ntohs(packet->DESTINATION_PORT);
	packet->FLAGS = packet->FLAGS;
	packet->SEQ_NUMBER = packet->SEQ_NUMBER;
	packet->PAYLOAD_LENGTH = ntohs(packet->PAYLOAD_LENGTH);
	packet->CHECKSUM = ntohs(packet->CHECKSUM);

	printf("SOURCE_PORT %u\nDESTINATION_PORT %u\nPAYLOAD_LENGTH %u\nFLAGS %u\nSEQ_NUMBER %u\nCHECKSUM %u\n\n",
		packet->SOURCE_PORT, packet->DESTINATION_PORT, packet->PAYLOAD_LENGTH, packet->FLAGS, packet->SEQ_NUMBER, packet->CHECKSUM);

	// Get payload from buffer
	void* payload = malloc(packet->PAYLOAD_LENGTH);
	memcpy(payload, buffer + ASP_PACKET_HEADER_SIZE, packet->PAYLOAD_LENGTH);
	packet->data = payload;

	if (!has_valid_checksum(packet)) {
		printf("invalid checksum!\n");
		return NULL;
	}
	return packet;
}