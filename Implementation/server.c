#include "wav_file.h"
#include "asp_socket.h"

// PROGRAM OPTIONS
bool VERBOSE_LOGGING = false;

void usage(const char* name) {
	fprintf(stderr, "Usage: %s [OPTION]... [file...]\n\t-v\tverbose packet logging\n", name);
	exit(-1);
}

asp_packet* get_ack(asp_socket* sock) {
	// Peek at the next incoming packet, because if it isn't an ACK or REJ, the packet will have to be parsed elsewhere
	void* buffer = receive_packet(sock, MSG_PEEK);
	asp_packet* packet = deserialize_asp(buffer);

	if (packet != NULL && (is_flag_set(packet, ACK) || is_flag_set(packet, REJ))) {
		// Remove the ACK packet from the receive queue, because we need to parse it
		void* ACK_packet = receive_packet(sock, 0);
		free(ACK_packet);
		return packet;
	}
	return NULL;
}

void start_streaming(asp_socket* sock, const struct wave_file *wf) {
	printf("Starting stream...\n");

	uint8_t* current_sample = wf->samples;
	uint32_t ASP_WINDOW_POS = 0;

	// Let the client know the stream is beginning
	asp_send_event(sock, NEXT_EVENT);

	// Send wav header
	asp_send_wav_header(sock, wf->wh);

	// Send wav samples
	uint32_t samples_done = 0;
	//uint32_t sample_size = wf->wh->w_bits_per_sample/8;
	uint32_t sample_size = 1;
	while (samples_done < wf->payload_size / sample_size) {
		// Cut samples up in blocks
		uint8_t* payload = malloc(ASP_PACKET_WAV_SAMPLES * sample_size);
		uint8_t* payload_pos = payload;

		switch (sock->info.current_quality_level) {
			case 1:
				break;
			case 2:
				break;
			case 3:
				break;
			case 4:
				// put ASP_PACKET_WAV_SAMPLES samples in payload
				for (uint32_t sample=0; sample < ASP_PACKET_WAV_SAMPLES; ++sample) {
					memcpy(payload_pos, current_sample, sample_size);
					payload_pos += sample_size;
					current_sample += sample_size;
					samples_done++;

					//current_sample += sample_size;
					//samples_done++;
				}
				break;
			case 5:
				// put all ASP_PACKET_WAV_SAMPLES directly into the payload
				memcpy(payload_pos, current_sample, ASP_PACKET_WAV_SAMPLES * sample_size);
				payload_pos += ASP_PACKET_WAV_SAMPLES * sample_size;
				current_sample += ASP_PACKET_WAV_SAMPLES * sample_size;
				samples_done += ASP_PACKET_WAV_SAMPLES;
				break;
			default: {
				fprintf(stderr, "Invalid socket streaming quality level! level: %u", sock->info.current_quality_level);
				return;
			}
		}

		// Send packet
		asp_send_wav_samples(sock, payload, ASP_PACKET_WAV_SAMPLES, ASP_WINDOW_POS);
		free(payload);

		// Wait for acknowledgement from client when needed
		if (++ASP_WINDOW_POS >= ASP_WINDOW) {
			asp_packet* packet = get_ack(sock);
			if (is_flag_set(packet, ACK)) {
				ASP_WINDOW_POS = 0;
			}
			else if (is_flag_set(packet, REJ)) {
				ASP_WINDOW_POS = 0;
				uint16_t first_missing_packet = *(uint16_t*)packet->data + 1;
				current_sample -= (ASP_WINDOW - first_missing_packet + 1) * (ASP_PACKET_WAV_SAMPLES * sizeof(uint8_t));
			}
			else return;
		}
	}

	// Let the client know the stream is ending
	asp_send_event(sock, NEXT_EVENT);

	printf("Finished streaming the file.\n");
}

int main(int argc, char **argv) {
	// Argument handling
	if (argc < 2) usage(argv[0]);
	int opt;
	while ((opt = getopt(argc, argv, "vh")) != -1) {
		switch (opt) {
			case 'v': VERBOSE_LOGGING = true;
				break;
			default: usage(argv[0]);
		}
	}
	char* filename = argv[optind];

	// Open the WAVE file
	struct wave_file wf = {0,};
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

		if (packet != NULL && is_flag_set(packet, NEW_CLIENT)) {
			uint32_t buffer_size = *(uint32_t*)packet->data;
			start_streaming(&sock, &wf);
		}
		else fprintf(stderr, "the packet was invalid.\n");
		free(packet);
	}

	/* Clean up */
	destroy_socket(&sock);
	close_wave_file(&wf);

	return 0;
}