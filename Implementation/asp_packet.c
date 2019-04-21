#include "asp_packet.h"

uint16_t size(asp_packet * packet) {
	return (4 * sizeof(uint16_t)) + packet->PAYLOAD_LENGTH;
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

bool has_valid_checksum(asp_packet* packet) {
	return (packet->CHECKSUM == 5);
}

asp_packet create_asp_packet(uint16_t source, uint16_t dest, void* data, uint16_t data_size) {
	asp_packet packet;

	packet.SOURCE_PORT = source;
	packet.DESTINATION_PORT = dest;
	packet.PAYLOAD_LENGTH = data_size;
	packet.CHECKSUM = get_checksum(data);
	packet.data = data;

	return packet;
}

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

asp_packet* deserialize_asp(void* buffer) {
	asp_packet* packet = buffer;

	// Convert header from network to host
	packet->SOURCE_PORT = ntohs(packet->SOURCE_PORT);
	packet->DESTINATION_PORT = ntohs(packet->DESTINATION_PORT);
	packet->PAYLOAD_LENGTH = ntohs(packet->PAYLOAD_LENGTH);
	packet->CHECKSUM = ntohs(packet->CHECKSUM);

	// Get payload from buffer
	char* message = malloc(packet->PAYLOAD_LENGTH);
	memcpy(message, buffer + (4 * sizeof(uint16_t)), packet->PAYLOAD_LENGTH);
	packet->data = message;

	if (!has_valid_checksum(packet)) return NULL;
	return packet;
}