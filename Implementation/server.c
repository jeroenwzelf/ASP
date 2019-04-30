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

void get_ack(asp_socket* sock) {
	while (true) {
		void* buffer = receive_packet(sock);
		asp_packet* packet = deserialize_asp(buffer);

		if (strcmp(packet->data, "ACK") == 0)
			return;
	}
}

void send_wav_samples(asp_socket* sock, struct wave_file *wf, uint32_t BLOCK_SIZE) {
	uint8_t* current_sample = wf->samples;
	uint32_t packets_sent = 0;

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

		if (++packets_sent > ASP_WINDOW) {
			get_ack(sock);
			packets_sent = 0;
		}

		current_sample += BLOCK_SIZE * sizeof(uint8_t);

		free(payload);
	}
}

void start_streaming(asp_socket* sock, struct wave_file *wf, uint32_t BLOCK_SIZE) {
	printf("Starting stream...\n");
	send_wav_header(sock, wf->wh);
	send_wav_samples(sock, wf, BLOCK_SIZE);

	asp_packet packet = create_asp_packet(
				ntohs(sock->info.local_addr.sin_port),
				ntohs(sock->info.remote_addr.sin_port), 
				"SS", 6); // "SS\0" is 3 char long, and sizeof(char) == 2 * sizeof(uint16_t), so length in uint16_t octets is 2 * 3 = 6
	send_packet(sock, serialize_asp(&packet), size(&packet));

	printf("Finished streaming the file.\n");
}

int main(int argc, char **argv) {
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
		printf("Listening for incoming client...\n");

		void* buffer = receive_packet(&sock);
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