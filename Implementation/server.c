#include "wav_file.h"
#include "asp_socket.h"

static struct wave_file wf = {0,};

void usage(char* name) {
	fprintf(stderr, "Usage: %s [file...]\n", name);
	exit(-1);
}

void send_wav_header(asp_socket* sock, struct wave_header *wh) {
	void* buffer = serialize_wav_header(wh);
	asp_packet packet = create_asp_packet(
				ntohs(sock->info.local_addr.sin_port),
				ntohs(sock->info.remote_addr.sin_port),
				buffer, sizeof_wav_header());
	send_packet(sock, serialize_asp(&packet), size(&packet));
	free(buffer);
}

bool get_ack(asp_socket* sock) {
	while (true) {
		// Peek at the next incoming packet, because if it isn't an ACK, the packet will have to be parsed elsewhere
		void* buffer = receive_packet(sock, MSG_PEEK);
		asp_packet* packet = deserialize_asp(buffer);

		if (packet != NULL) {
			bool IS_ACK = strcmp(packet->data, "ACK");
			free(buffer);

			if (IS_ACK == 0) {
				// Remove the ACK packet from the receive queue
				void* ACK_packet = receive_packet(sock, 0);
				free(ACK_packet);
				return true;
			}
			return false;
		}
	}
}

void send_wav_samples(asp_socket* sock, struct wave_file *wf, uint32_t BLOCK_SIZE) {
	uint8_t* current_sample = wf->samples;
	uint32_t ASP_WINDOW_POS = 0;

	printf("%u\n", BLOCK_SIZE);
	printf("%u\n", wf->payload_size/BLOCK_SIZE);

	for (uint32_t i=0; i<wf->payload_size/BLOCK_SIZE; ++i) {
		// Cut samples up in blocks of BLOCK_SIZE
		uint8_t* payload = malloc(BLOCK_SIZE * sizeof(uint8_t));
		memcpy(payload, current_sample, BLOCK_SIZE * sizeof(uint8_t));

		// Send packet
		asp_packet packet = create_asp_packet(
				ntohs(sock->info.local_addr.sin_port),
				ntohs(sock->info.remote_addr.sin_port),
				payload, BLOCK_SIZE * sizeof(uint8_t));
		send_packet(sock, serialize_asp(&packet), size(&packet));

		// Wait for acknowledgement from client
		if (++ASP_WINDOW_POS > ASP_WINDOW) {
			if (!get_ack(sock)) return;
			ASP_WINDOW_POS = 0;
		}

		// Goto next sample
		current_sample += BLOCK_SIZE * sizeof(uint8_t);

		free(payload);
	}
}

void start_streaming(asp_socket* sock, struct wave_file *wf, uint32_t BLOCK_SIZE) {
	printf("Starting stream...\n");

	// Let the client know the stream is beginning
	asp_packet bs_packet = create_asp_packet(
				ntohs(sock->info.local_addr.sin_port),
				ntohs(sock->info.remote_addr.sin_port), 
				"BS", 6); // "BS\0" is 3 char long, and sizeof(char) == 2 * sizeof(uint16_t), so length in uint16_t octets is 2 * 3 = 6
	send_packet(sock, serialize_asp(&bs_packet), size(&bs_packet));

	// Send wav header + wav samples
	send_wav_header(sock, wf->wh);
	send_wav_samples(sock, wf, BLOCK_SIZE);

	// Let the client know the stream is ending
	asp_packet es_packet = create_asp_packet(
				ntohs(sock->info.local_addr.sin_port),
				ntohs(sock->info.remote_addr.sin_port), 
				"ES", 6); // "BS\0" is 3 char long, and sizeof(char) == 2 * sizeof(uint16_t), so length in uint16_t octets is 2 * 3 = 6
	send_packet(sock, serialize_asp(&es_packet), size(&es_packet));

	printf("Finished streaming the file.\n");
}

int main(int argc, char **argv) {
	// Argument handling
	if (argc < 2) usage(argv[0]);
	int opt;
	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
			default: usage(argv[0]);
		}
	}
	char* filename = argv[optind];

	// Open the WAVE file
	if (open_wave_file(&wf, filename) < 0) return -1;
	
	// Set up network connection (open socket)
	asp_socket sock = new_socket(ASP_SERVER_PORT);

	// Wait for anyone to connect to the socket, then send an audio stream
	while (true) {
		if (sock.state != WORKING) exit(1);
		printf("Listening for incoming client...\n");

		// The first packet of a client contains its recv buffer size.
		// For now, we intend to fill that buffer, so this will tell us how large to make our packets.
		void* buffer = receive_packet(&sock, 0);
		asp_packet* packet = deserialize_asp(buffer);

		if (packet != NULL) {
			uint32_t buffer_size = *(uint32_t*)packet->data;
			free(buffer);
			start_streaming(&sock, &wf, buffer_size);
		}
		else printf("The packet was invalid!\n");
	}

	/* Clean up */
	close_wave_file(&wf);

	return 0;
}