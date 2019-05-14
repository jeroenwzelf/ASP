#include "wav_file.h"
#include "asp_socket.h"

// PROGRAM OPTIONS
#define SIM_CONNECTION true
bool VERBOSE_LOGGING = false;

void simulate_connection(asp_socket * sock, uint8_t *payload, const uint32_t BLOCK_SIZE, const uint32_t ASP_WINDOW_POS){
	// Simulating a bad connection is done by not sending every packet, thus having to send the packets again
	// after getting a rejection flag from the client.
	int percentage = rand() % (10 + 1 - 0) + 0;
	printf("%i\n",percentage );

	// There is a 10% chance that a packet will not be send, simulating that is has been lost in transfer
	if(percentage > 0)
		asp_send_wav_samples(sock,payload,BLOCK_SIZE,ASP_WINDOW_POS);
	else
		printf("%s\n", "hihi not sending ;)" );
}


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

void start_streaming(asp_socket* sock, const struct wave_file *wf, const uint32_t BLOCK_SIZE) {
	printf("Starting stream...\n");

	uint8_t* current_sample = wf->samples;
	uint32_t ASP_WINDOW_POS = 0;

	// Let the client know the stream is beginning
	asp_send_event(sock, NEXT_EVENT);

	// Send wav header
	asp_send_wav_header(sock, wf->wh);

	// Send wav samples
	for (uint32_t i=0; i<wf->payload_size/BLOCK_SIZE; ++i) {
		// Cut samples up in blocks of BLOCK_SIZE
		uint8_t* payload = malloc(BLOCK_SIZE * sizeof(uint8_t));
		memcpy(payload, current_sample, BLOCK_SIZE * sizeof(uint8_t));

		// Before sending a packet, check whether the boolean SIM_CONNECTION is true. If it is we are simulating a bad connection
		// and the sending of packets is done by the function simulate_connection().
		if(SIM_CONNECTION){
			simulate_connection(sock, payload,BLOCK_SIZE, ASP_WINDOW_POS);
		}
		
		else{
			asp_send_wav_samples(sock, payload, BLOCK_SIZE, ASP_WINDOW_POS);
		}

		free(payload);
		// Send packet
		
		// Wait for acknowledgement from client when needed
		if (++ASP_WINDOW_POS >= ASP_WINDOW) {
			asp_packet* packet = get_ack(sock);
			if (is_flag_set(packet, ACK)) {
				ASP_WINDOW_POS = 0;
			}
			else if (is_flag_set(packet, REJ)) {
				ASP_WINDOW_POS = 0;
				printf("%u\n", *(uint16_t*)packet->data);
				uint16_t first_missing_packet = *(uint16_t*)packet->data;
				current_sample -= (ASP_WINDOW - first_missing_packet) * (BLOCK_SIZE * sizeof(uint8_t));
				i -= ASP_WINDOW - first_missing_packet;
			}
			else return;
		}

		// Goto next sample
		current_sample += BLOCK_SIZE * sizeof(uint8_t);
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
			start_streaming(&sock, &wf, buffer_size);
		}
		else printf("The packet was invalid!\n");
		free(buffer);
	}

	/* Clean up */
	destroy_socket(&sock);
	close_wave_file(&wf);

	return 0;
}