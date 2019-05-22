#include "asp_socket.h"
#include "wav_header.h"

#include <alsa/asoundlib.h>

// PROGRAM OPTIONS
bool VERBOSE_LOGGING = false;
uint32_t buffer_size = 1024;
uint8_t ack_count;
uint8_t rej_count;
static char SERVER_IP[16];	// largest length for ip: sizeof(XXX.XXX.XXX.XXX\0) == 16

void usage(const char* name) {
	fprintf(stderr, "  Usage: %s [OPTION]... [-b buffer (> 16)] [-p client-port] [-s server-ip-address]\n\t-v\tverbose packet logging\n", name);
	exit(-1);
}

struct wave_header* initial_server_handshake(asp_socket* sock) {
	asp_send_client_info(sock, buffer_size);
	ack_count = 60;
	rej_count = 0;
	// Wait for server to be ready to start streaming
	bool got_new_port = false;
	while (!got_new_port) {
		// Old packets from a previous client may be coming through
		void* buffer = receive_packet(sock, 0);
		asp_packet* packet = deserialize_asp(buffer);

		// Connect to new port
		if (packet != NULL && is_flag_set(packet, NEW_CLIENT)) {
			uint32_t new_port = htons(*(uint32_t*)packet->data);
			set_remote_addr(sock, SERVER_IP, new_port);
			got_new_port = true;
			printf("switching outgoing port to %u\n", new_port);
		}
		free(packet);
	}

	// Receive the header of the wav file
	void* buffer = receive_packet(sock, 0);
	asp_packet* packet = deserialize_asp(buffer);

	if (packet == NULL) {
		fprintf(stderr, "couldn't perform initial handshake (no header with audio stream information).\n");
		return NULL;
	}

	struct wave_header* header = deserialize_wav_header(packet->data);
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

void filter_old_packet_requests(asp_socket* sock) {
	while (true) {
		void* buffer = receive_packet(sock, MSG_PEEK);
		asp_packet* packet = deserialize_asp(buffer);

		if (packet != NULL && is_flag_set(packet, DATA_WAV_SAMPLES)) {
			uint8_t SEQ_NUMBER = packet->SEQ_NUMBER;
			free(packet);
			if (SEQ_NUMBER == 0) return;	// it was a packet from a new sequence!
			
			// remove old packet from recv buffer
			void* buffer = receive_packet(sock, 0);
			free(buffer);
		}
	}
}

asp_packet* receive_wav_samples(asp_socket* sock) {
	void* buffer = receive_packet(sock, 0);
	asp_packet* packet = deserialize_asp(buffer);

	if (packet != NULL && is_flag_set(packet, DATA_WAV_SAMPLES)) {
		// Check if we are still getting the expected packet order
		// If our counter is less than the packet, we missed some packets
		if (sock->info.sequence_count++ < packet->SEQ_NUMBER) {
			ack_count = 0;
			++rej_count;
			if(rej_count > 4){
				if(sock->info.current_quality_level > 1){
					--sock->info.current_quality_level;
					printf("%s\n","Decreasing the Audio quality");
					asp_send_rej_quality_down(sock, sock->info.sequence_count);
				}
				else asp_send_rejection(sock, sock->info.sequence_count);
			}
			else asp_send_rejection(sock, sock->info.sequence_count);
			sock->info.sequence_count = 0;

			filter_old_packet_requests(sock);
			return receive_wav_samples(sock);
		}
		else if (sock->info.sequence_count >= ASP_WINDOW) {
			++ack_count;
			if(ack_count > 50){
				if(sock->info.current_quality_level < 5){
					++sock->info.current_quality_level;
					ack_count = 0;
					printf("%s\n","Increasing the Audio quality");
					asp_send_event(sock, ACK_QUALITY_UP);
				}
				else asp_send_event(sock, ACK);
			}
			else asp_send_event(sock, ACK);
			sock->info.sequence_count = 0;
		}
	}
	return packet;
}

bool fill_buffer(asp_socket* sock, uint8_t* recv_ptr, uint32_t sample_size) {
	uint32_t recv_filled = 0;

	while (recv_filled < buffer_size) {
		asp_packet* wav_sample_packet = receive_wav_samples(sock);
		if (wav_sample_packet == NULL || !is_flag_set(wav_sample_packet, DATA_WAV_SAMPLES))
			return false;

		uint8_t* wav_samples = wav_sample_packet->data;
		// there are always ASP_PACKET_WAV_SAMPLES samples in a packet
		// depending on the quality, some samples should be copied to keep the audio framerate thesame
		uint8_t downsampling = get_downsampling_quality(sock->info.current_quality_level,buffer_size,sample_size);
		for (uint32_t sample=0; sample < ASP_PACKET_WAV_SAMPLES; ++sample) {
			for (uint8_t copy=0; copy < downsampling; ++copy) {
				memcpy(recv_ptr, wav_samples, sample_size);
				recv_ptr += sample_size;
				recv_filled += sample_size;
			}
			wav_samples += sample_size;
		}
		free(wav_sample_packet);
	}
	return true;
}

void stream_wav_file(asp_socket* sock) {
	// connect to server and get audio stream information
	struct wave_header* header = initial_server_handshake(sock);
	if (header == NULL) return;

	// configure audio device
	snd_pcm_t* snd_handle = open_audio_device(header->n_channels, header->n_samples_per_sec);
	if (snd_handle == NULL) return;

	bool stream_stopped = false;

	// set up buffers/queues
	uint8_t* recvbuffer = malloc(buffer_size);
	uint8_t* playbuffer = malloc(buffer_size);
	uint8_t* play_ptr = playbuffer;
	
	// fill the recvbuffer first
	if (!fill_buffer(sock, recvbuffer, header->w_bits_per_sample/8)) stream_stopped = true;

	// Play the wav file
	while (!stream_stopped) {
		int i = 0;
		if (i <= 0) {
			// Transfer recvbuffer to playbuffer, get a new recvbuffer
			memcpy(playbuffer, recvbuffer, buffer_size);
			if (!fill_buffer(sock, recvbuffer, header->w_bits_per_sample/8)) stream_stopped = true;

			// Reset playbuffer
			play_ptr = playbuffer;
			i = buffer_size;
		}

		/* write frames to ALSA */
		//snd_pcm_sframes_t frames = snd_pcm_writei(snd_handle, play_ptr, (buffer_size - ((int) play_ptr - (int) playbuffer)) / header->n_block_align);
		snd_pcm_sframes_t frames = snd_pcm_writei(snd_handle, play_ptr,
			(buffer_size - ((int) play_ptr - (int) playbuffer)) / header->n_block_align);

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
	uint32_t CLIENT_PORT = 0;	// default: bind to any available port

	int opt;
	while ((opt = getopt(argc, argv, "p:b:s:vh")) != -1) {
		switch (opt) {
			case 'v': VERBOSE_LOGGING = true; break;
			case 'p': CLIENT_PORT = atoi(optarg); break;
			case 'b':
				buffer_size = atoi(optarg);
				// round buffer size up to the nearest power of 2
				buffer_size--;
				buffer_size |= buffer_size >> 1;
				buffer_size |= buffer_size >> 2;
				buffer_size |= buffer_size >> 4;
				buffer_size |= buffer_size >> 8;
				buffer_size |= buffer_size >> 16;
				buffer_size++;
				break;
			case 's': snprintf(SERVER_IP, 16, "%s", optarg); break;
		default: usage(argv[0]);
		}
	}
	if (optind < argc) usage(argv[0]);

	// Set up network connection
	asp_socket sock = new_socket(0);
	set_remote_addr(&sock, SERVER_IP, ASP_SERVER_PORT);

	// Start audio streaming
	stream_wav_file(&sock);

	// Cleanup
	destroy_socket(&sock);

	return 0;
}