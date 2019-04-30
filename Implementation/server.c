#include "wav_file.h"
#include "asp.h"

static struct wave_file wf = {0,};

void usage(char* name) {
	fprintf(stderr, "Usage: %s [file...]\n", name);
	exit(-1);
}

void start_streaming(asp_socket* sock, struct wave_file *wf) {
	// First, send wav header
	void* buffer = serialize_wav_header(wf->wh);
	asp_packet packet = create_asp_packet(
				ntohs(sock->info.local_addr.sin_port),
				ntohs(sock->info.remote_addr.sin_port),
				buffer, sizeof_wav_header());
	send_packet(sock, serialize_asp(&packet), size(&packet));
	free(buffer);


	// Then send all samples
	uint32_t BLOCK_SIZE = 1024;
	for (uint32_t i=0; i<=wf->payload_size; ++i) {
		// Cut samples up in blocks of BLOCK_SIZE
		usleep(5000);	// temporary solution for the client not filling up his buffer correctly
		uint8_t* payload = malloc(BLOCK_SIZE * sizeof(uint8_t));
		memcpy(payload, wf->samples, BLOCK_SIZE * sizeof(uint8_t));

		// Send packet
		asp_packet packet = create_asp_packet(
				ntohs(sock->info.local_addr.sin_port),
				ntohs(sock->info.remote_addr.sin_port),
				payload, BLOCK_SIZE * sizeof(uint8_t));
		send_packet(sock, serialize_asp(&packet), size(&packet));

		wf->samples += BLOCK_SIZE * sizeof(uint8_t);

		free(payload);
	}
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
			int client_wants_audiostream = strcmp(packet->data, "SS");
			free(buffer);
			if (client_wants_audiostream == 0)
				start_streaming(&sock, &wf);
		}
		else printf("The packet was invalid!\n");
	}

	/* Clean up */
	close_wave_file(&wf);

	return 0;
}