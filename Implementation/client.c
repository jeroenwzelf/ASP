#include "asp_socket.h"
#include "wav_header.h"

#include <alsa/asoundlib.h>

// PROGRAM OPTIONS
bool VERBOSE_LOGGING = false;
static uint32_t buffer_size = 1024;
static char SERVER_IP[16];	// largest length for ip: sizeof(XXX.XXX.XXX.XXX\0) == 16

void usage(const char* name) {
	fprintf(stderr, "  Usage: %s [OPTION]... [-b buffer] [-s server-ip-address]\n\t-v\tverbose packet logging\n", name);
	exit(-1);
}

struct wave_header* receive_wav_header(asp_socket* sock) {
	void* buffer = receive_packet(sock, 0);
	asp_packet* packet = deserialize_asp(buffer);

	if (packet != NULL)
		return deserialize_wav_header(packet->data);
	return NULL;
}

struct wave_header* initial_server_handshake(asp_socket* sock) {
	asp_send_client_info(sock, buffer_size);

	// Wait for server to be ready to start streaming
	while (true) {
		// Old packets from a previous client may be coming through
		void* buffer = receive_packet(sock, 0);
		asp_packet* packet = deserialize_asp(buffer);

		if (packet != NULL) {
			bool start_stream = is_flag_set(packet, NEXT_EVENT);
			free(packet);
			if (start_stream) break;
		}
	}

	// Receive the header of the wav file
	struct wave_header* header = receive_wav_header(sock);
	if (header == NULL) {
		fprintf(stderr, "couldn't perform initial handshake (no header with audio stream information).\n");
		return NULL;
	}

	printf("%s:%d is streaming a wav file with ",
			inet_ntoa(sock->info.remote_addr.sin_addr), ntohs(sock->info.remote_addr.sin_port));
	printf("mode %s, samplerate %lu\n",
				header->n_channels == 2 ? "Stereo" : "Mono",
				header->n_samples_per_sec);

	return header;
}

snd_pcm_t* open_audio_device(const uint16_t n_channels, const uint32_t n_samples_per_sec) {
	/* Open audio device */
	snd_pcm_t* snd_handle;
	int err = snd_pcm_open(&snd_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);

	if (err < 0) {
		fprintf(stderr, "couldnt open audio device: %s\n", snd_strerror(err));
		return NULL;
	}

	/* Configure parameters of PCM output */
	err = snd_pcm_set_params(snd_handle,
								SND_PCM_FORMAT_S16_LE,
								SND_PCM_ACCESS_RW_INTERLEAVED,
								n_channels,
								n_samples_per_sec,
								0,          /* Allow software resampling */
								500000);    /* 0.5 seconds latency */
	if (err < 0) {
		printf("couldnt configure audio device: %s\n", snd_strerror(err));
		return NULL;
	}

	return snd_handle;
}

void* receive_wav_samples(asp_socket* sock) {
	void* buffer = receive_packet(sock, 0);
	asp_packet* packet = deserialize_asp(buffer);

	if (packet != NULL && is_flag_set(packet, DATA_WAV_SAMPLES)) {
		// Check if we are still getting the expected packet order
		// If our counter is less than the packet, we missed some packets
		if (++sock->info.sequence_count < packet->SEQ_NUMBER) {
			asp_send_rejection(sock, sock->info.sequence_count);
			sock->info.sequence_count = 0;
			return receive_wav_samples(sock);
		}
		else if (sock->info.sequence_count >= ASP_WINDOW) {
			asp_send_event(sock, ACK);
			sock->info.sequence_count = 0;
		}
		return packet->data;
	}
	return NULL;
}

void stream_wav_file(asp_socket* sock) {
	// connect to server and get audio stream information
	struct wave_header* header = initial_server_handshake(sock);
	if (header == NULL) return;

	// configure audio device
	snd_pcm_t* snd_handle = open_audio_device(header->n_channels, header->n_samples_per_sec);
	if (snd_handle == NULL) return;

	// set up buffers/queues
	uint8_t *recvbuffer, *playbuffer;
	uint8_t *recv_ptr, *play_ptr;
	recvbuffer = malloc(buffer_size);
	playbuffer = malloc(buffer_size);
	
	// fill the buffer
	memcpy(playbuffer, receive_wav_samples(sock), buffer_size);
	memcpy(recvbuffer, receive_wav_samples(sock), buffer_size);

	// Play the wav file
	printf("playing...\n");

	int i = 0;
	recv_ptr = recvbuffer;
	while (true) {
		if (i <= 0) {
			play_ptr = playbuffer;
			i = buffer_size;

			memcpy(playbuffer, recvbuffer, buffer_size);
			void* wav_samples = receive_wav_samples(sock);
			if (wav_samples == NULL) break;
			memcpy(recvbuffer, wav_samples, buffer_size);
		}

		/* write frames to ALSA */
		snd_pcm_sframes_t frames = snd_pcm_writei(snd_handle, play_ptr, (buffer_size - ((int) play_ptr - (int) playbuffer)) / header->n_block_align);

		/* Check for errors */
		int ret = 0;
		if (frames < 0)
			ret = snd_pcm_recover(snd_handle, frames, 0);
		if (ret < 0) {
			fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %i\n", ret);
			exit(EXIT_FAILURE);
		}
		if (frames > 0 && frames < (buffer_size - ((int) play_ptr - (int) playbuffer)) / header->n_block_align)
			printf("Short write (expected %i, wrote %li)\n",
						(int) (buffer_size - ((int) play_ptr - (int) playbuffer)) / header->n_block_align, frames);

		/* advance pointers accordingly */
		if (frames > 0) {
			play_ptr += frames * header->n_block_align;
			i -= frames * header->n_block_align;
		}

		if ((int) play_ptr - (int) playbuffer == buffer_size)
			i = 0;
	}

	/* clean up */
	free(recvbuffer);
	free(playbuffer);

	snd_pcm_drain(snd_handle);
	snd_pcm_hw_free(snd_handle);
	snd_pcm_close(snd_handle);
}

int main(int argc, char **argv) {
	snprintf(SERVER_IP, 16, "%s", "127.0.0.1"); // default server ip

	int opt;
	while ((opt = getopt(argc, argv, "b:s:vh")) != -1) {
		switch (opt) {
			case 'v': VERBOSE_LOGGING = true;
				break;
			case 'b':
				buffer_size = atoi(optarg);
				break;
			case 's':
				snprintf(SERVER_IP, 16, "%s", optarg);
				break;
		default: usage(argv[0]);
		}
	}
	if (optind < argc) usage(argv[0]);

	// Set up network connection
	asp_socket sock = new_socket(1234);
	set_remote_addr(&sock, SERVER_IP, ASP_SERVER_PORT);

	// Start audio streaming
	stream_wav_file(&sock);

	// Cleanup
	destroy_socket(&sock);

	return 0;
}