#include "asp_packet.h"

uint16_t ones_complement_sum(asp_packet* packet) {
	if (packet == NULL) return 0;
	/* From RFC 1071: Adjacent octets are paired to form 8-bit
	integers, and the 1's complement sum of these 8-bit integers is
	formed. */
	register uint16_t sum = 0;
	uint16_t* data = packet->data;

	for (uint8_t  i=0; i<ASP_PACKET_HEADER_SIZE; i+=2) sum += ((uint16_t*)packet)[i/2];	// header
	for (uint16_t i=0; i<packet->PAYLOAD_LENGTH; i+=2) sum += *(data++);				// payload
	return sum;
}

bool has_valid_checksum(asp_packet* packet) {
	if (packet == NULL) return false;
	/* From RFC 1071: To check a checksum, the 1's complement sum is computed over the
	same set of octets, including the checksum field.  If the result
	is all 1 bits (-0 in 1's complement arithmetic), the check
	succeeds. */

	// Note: all 1 bits is -1 in 2's complement arithmetic
	return ((int16_t)ones_complement_sum(packet) == -1);
}

asp_packet create_asp_packet(uint16_t source, uint16_t dest, uint8_t flags, uint8_t seq_number, void* data, uint16_t data_size) {
	asp_packet packet;

	packet.SOURCE_PORT = source;
	packet.DESTINATION_PORT = dest;

	packet.FLAGS = flags;
	packet.SEQ_NUMBER = seq_number;

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
	return ASP_PACKET_HEADER_SIZE + packet->PAYLOAD_LENGTH;
}

void print_packet(asp_packet* packet) {
	if (packet == NULL) return;
	printf("Source Port %u, Destination Port %u\nFlags ", packet->SOURCE_PORT, packet->DESTINATION_PORT, packet->FLAGS);
	print_flags(packet->FLAGS);
	printf(", Seq# %u, Length %u, Checksum %u\n\n", packet->SEQ_NUMBER, packet->PAYLOAD_LENGTH, packet->CHECKSUM);
}

void print_flags(uint8_t flags) {
	bool flag_before_happened = false;
	for (uint8_t bit = 0; bit < 8; ++bit) {
		if (flags & 1) {
			if (flag_before_happened) printf("|");
			switch (bit) {
				case 0: printf("%s", "ACK"); break;
				case 1: printf("%s", "REJ"); break;
				case 2: printf("%s", "NEW_CLIENT"); break;
				case 3: printf("%s", "DATA_WAV_HEADER"); break;
				case 4: printf("%s", "DATA_WAV_SAMPLES"); break;
				case 5: printf("%s", "NEXT_EVENT"); break;
				case 6: printf("%s", "UNSPECIFIED"); break;
				case 7: printf("%s", "UNSPECIFIED"); break;
			}
		}
		else flag_before_happened = false;
		flags >>= 1;
	}
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

	// Get payload from buffer
	void* payload = malloc(packet->PAYLOAD_LENGTH);
	memcpy(payload, buffer + ASP_PACKET_HEADER_SIZE, packet->PAYLOAD_LENGTH);
	packet->data = payload;

	if (VERBOSE_LOGGING) print_packet(packet);

	if (!has_valid_checksum(packet)) {
		printf("A packet arrived with an invalid checksum.\n");
		return NULL;
	}
	return packet;
}