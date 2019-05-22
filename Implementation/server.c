#include "wav_file.h"
#include "asp_socket.h"

// PROGRAM OPTIONS
#define SIM_PACKET_LOSS 1
bool SIMULATION = false;
bool VERBOSE_LOGGING = false;
bool NO_LOGGING = false;

// wave file
struct wave_file wf = {0,};
// list of clients
asp_socket_list* client_list = NULL;
uint16_t client_list_size = 0;

void usage(const char* name) {
	fprintf(stderr, "Usage: %s [OPTION]... [file...]\n\t-s\tenable bad connection simulation\n\t-v\tverbose packet logging\n\t-n\tno logging\n", name);
	exit(-1);
}

bool simulate_bad_connection_tick() {
	if (!SIMULATION) return false;
	// Simulating a bad connection is done by not sending every packet, thus having to send the packets again
	// after getting a rejection flag from the client.

	// There is a SIM_PACKET_LOSS% chance that a packet will not be sent, simulating that is has been lost in transfer
	return ( rand() % 1000 ) <= SIM_PACKET_LOSS;
}

void send_wav_sample_batch(asp_socket* sock) {
	for (uint32_t ASP_WINDOW_POS=0; ASP_WINDOW_POS < ASP_WINDOW; ++ASP_WINDOW_POS) {
		if (sock->stream->samples_done < wf.payload_size / sock->stream->sample_size) {
			// Cut samples up in blocks
			uint8_t* payload = malloc(ASP_PACKET_WAV_SAMPLES * sock->stream->sample_size);
			uint8_t* payload_pos = payload;

			uint8_t downsampling = get_downsampling_quality(sock->info.current_quality_level, sock->stream->client_buffer_size, sock->stream->sample_size);
			for (uint32_t sample=0; sample < ASP_PACKET_WAV_SAMPLES; ++sample) {
				memcpy(payload_pos, sock->stream->current_sample, sock->stream->sample_size);
				payload_pos += sock->stream->sample_size;
				for (uint8_t copy=0; copy < downsampling; ++copy) {
					sock->stream->current_sample += sock->stream->sample_size;
					++sock->stream->samples_done;
				}
			}

			// Send packet
			if (!simulate_bad_connection_tick() || ASP_WINDOW_POS == ASP_WINDOW - 1 || ASP_WINDOW_POS == 0)
				asp_send_wav_samples(sock, payload, ASP_PACKET_WAV_SAMPLES * sock->stream->sample_size, ASP_WINDOW_POS);
			else if (!NO_LOGGING) printf("Simulated a bad connection by not sending a packet.\n");

			free(payload);
		}
		// end stream
		else {
			asp_send_event(sock, NEXT_EVENT);
			printf("Finished streaming the file for client %u:%u.\n", sock->info.remote_addr.sin_addr, ntohs(sock->info.local_addr.sin_port));
			return;
		}
	}
}

void setup_stream(asp_socket* sock, const uint32_t client_buffer_size) {
	sock->stream = malloc(sizeof(asp_socket_stream));
	sock->stream->current_sample = wf.samples;
	sock->stream->samples_done = 0;
	sock->stream->sample_size = wf.wh->w_bits_per_sample/8;
	sock->stream->client_buffer_size = client_buffer_size;

	asp_send_wav_header(sock, wf.wh);
	send_wav_sample_batch(sock);
}

void add_new_client_list(const asp_socket* sock) {
	asp_socket_list* client = client_list;
	while (client->next != NULL) client = client->next;

	client->next = malloc(sizeof(asp_socket_list));
	client->next->sock = sock;
	client->next->next = NULL;

	++client_list_size;
	printf("Server is now streaming to %u client%s\n", client_list_size, (client_list_size != 1) ? "s" : "");
}

void EVENT_new_client(asp_socket* sock, asp_packet* packet) {
	char* client_IP = socket_address_string(sock->info.remote_addr);
	printf("New client request from address %s:%u\n", client_IP, packet->SOURCE_PORT);

	asp_socket* client_sock = malloc(sizeof(asp_socket));
	*client_sock = new_socket(0);	// a new socket on any available port
	set_socket_timeout(client_sock, 0, 10);	// If there is no packet, stop the blocking recv call after 1 microsecond
	if (client_sock->state != WORKING)
		fprintf(stderr, "Could not create a new socket. Incoming client ignored.\n");
	else {
		set_remote_addr(client_sock, client_IP, packet->SOURCE_PORT);

		add_new_client_list(client_sock);
		asp_send_client_info(sock, client_sock->info.local_addr.sin_port);
		// The first packet of a client contains its recv buffer size.
		// At least one packet of audio samples has to be able to fit in this buffer.
		setup_stream(client_sock, *(uint32_t*)packet->data);
	}
}

void EVENT_ACK(asp_socket* sock) {
	if (sock->stream == NULL) return;
	send_wav_sample_batch(sock);
}

void EVENT_REJ(asp_socket* sock, asp_packet* packet) {
	if (sock->stream == NULL) return;

	uint16_t first_missing_packet = *(uint16_t*)packet->data;
	sock->stream->current_sample -= (ASP_WINDOW - first_missing_packet) * (ASP_PACKET_WAV_SAMPLES * sock->stream->sample_size);
	sock->stream->samples_done -=  (ASP_WINDOW - first_missing_packet) * ASP_PACKET_WAV_SAMPLES;
	send_wav_sample_batch(sock);
}

void get_requests_loop() {
	while (true) {
		uint32_t clientnumber = 0;
		asp_socket_list* client = client_list;
		while (client != NULL) {
			void* buffer = receive_packet(client->sock, 0);
			asp_packet* packet = deserialize_asp(buffer);

			if (packet != NULL) {
				// Quality flag
				if (is_flag_set(packet, QUALITY_UP) && (client->sock->info.current_quality_level < 5))
					++client->sock->info.current_quality_level;
				else if (is_flag_set(packet, QUALITY_DOWN) && (client->sock->info.current_quality_level > 1))
					--client->sock->info.current_quality_level;

				// Any other flag
				if (is_flag_set(packet, NEW_CLIENT)) EVENT_new_client(client->sock, packet);
				else if (is_flag_set(packet, ACK)) EVENT_ACK(client->sock);
				else if (is_flag_set(packet, REJ)) EVENT_REJ(client->sock, packet);
			}
			free(packet);

			client = client->next;
			++clientnumber;
		}
	}
}

int main(int argc, char **argv) {
	srand(time(NULL));

	// Argument handling
	if (argc < 2) usage(argv[0]);
	int opt;
	while ((opt = getopt(argc, argv, "svnh")) != -1) {
		switch (opt) {
			case 's': SIMULATION = true; break;
			case 'v': VERBOSE_LOGGING = true; break;
			case 'n': NO_LOGGING = true; break;
			default: usage(argv[0]);
		}
	}
	char* filename = argv[optind];

	// Open the WAVE file
	if (open_wave_file(&wf, filename) < 0) return -1;

	// Set up network connection (open socket)
	asp_socket sock = new_socket(ASP_SERVER_PORT);
	set_socket_timeout(&sock, 0, 10);	// If there is no packet, stop the blocking recv call after 1 microsecond
	if (sock.state != WORKING) {
		fprintf(stderr, "Could not create a new socket.\n");
		exit(1);
	}
	
	// Set up the client list
	client_list = malloc(sizeof(asp_socket_list));
	client_list->sock = &sock;
	client_list->next = NULL;
	
	get_requests_loop();

	/* Clean up */
	while (client_list != NULL) {
		asp_socket_list* client = client_list->next;
		destroy_socket(client_list->sock);
		free(client_list);
		client_list = client;
	}
	close_wave_file(&wf);

	return 0;
}