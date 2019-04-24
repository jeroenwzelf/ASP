#include "wav_file.h"
#include "asp.h"

#include <unistd.h>
#include <stdbool.h>

static struct wave_file wf = {0,};

void send_asp_packet(asp_socket* sock, void* message, uint16_t message_size) {
	if (sock->state == WORKING) {
		asp_packet packet = create_asp_packet(ntohs(sock->info.local_addr.sin_port), ntohs(sock->info.remote_addr.sin_port), message, message_size);
		send_packet(sock, serialize_asp(&packet), size(&packet));
	}
	else fprintf(stderr, "Couldn't send packet: socket is invalid!\n");
}

void start_streaming(asp_socket* sock, struct wave_file *wf) {
	// First, send wav header
	void* buffer = serialize_wav_header(wf->wh);
	send_asp_packet(sock, buffer, sizeof_wav_header());
	free(buffer);

	// Then send all samples
	for (uint32_t i=0; i<=wf->payload_size; ++i) {
		// Cut samples up in blocks of 1024
		usleep(5000);
		uint8_t* payload = malloc(1024 * sizeof(uint8_t));
		memcpy(payload, wf->samples, 1024 * sizeof(uint8_t));

		// Send packet
		send_asp_packet(sock, payload, 1024 * sizeof(uint8_t));
		wf->samples += 1024 * sizeof(uint8_t);

		free(payload);
	}
}

int main(int argc, char **argv) {
    if (argc < 2) {
    	fprintf(stderr, "Usage: %s [file...]\n", argv[0]);
    	return -1;
    }
    int opt;
    while ((opt = getopt(argc, argv, "")) != -1) {
        switch (opt) {
        default:
            fprintf(stderr, "Usage: %s [file...]\n", argv[0]);
            return -1;
        }
    }

	char* filename = argv[optind];

	// Open the WAVE file
	if (open_wave_file(&wf, filename) < 0) return -1;

	// Set up network connection (open socket)
	asp_socket sock = new_socket(ASP_SERVER_PORT);
	
	// Wait for anyone to connect to the socket, then send an audio stream
	while (true) {
		printf("Listening for incoming clients...\n");

		void* buffer = receive_packet(&sock);
		asp_packet* packet = deserialize_asp(buffer);

		if (packet != NULL) {
			int client_wants_audiostream = strcmp(packet->data, "SS");
			free(buffer);
			if (client_wants_audiostream == 0)
				start_streaming(&sock, &wf);
		}
		else {
			printf("The packet was invalid!\n");
		}
	}

	/* Clean up */
	close_wave_file(&wf);

	return 0;
}