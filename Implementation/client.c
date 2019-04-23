/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 */
#include "asp.h"
#include "wav_header.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>

#include <alsa/asoundlib.h>

/* 1 Frame = Stereo 16 bit = 32 bit = 4kbit 
#define FRAME_SIZE 4*/

void usage(char* name) {
	fprintf(stderr, "Usage: %s [-b buffer] [-s server ip-address] \n", name);
	exit(-1);
}

void send_asp_packet(asp_socket* sock, char* message) {
	if (sock->state == WORKING) {
		asp_packet packet = create_asp_packet(ntohs(sock->info.local_addr.sin_port), ntohs(sock->info.remote_addr.sin_port), message, strlen(message));
		send_packet(sock, serialize_asp(&packet), size(&packet));
	}
	else fprintf(stderr, "Couldn't send packet: socket is invalid!\n");
}

/* valid ip is of the form:
	A.B.C.D where A,B,C,D are numbers ranging from 0 to 999 */
bool valid_ip(char* ip) {
	uint16_t count_dots = 0;
	uint16_t count_nums = 0;
	bool previous_was_dot = false;

	char* ip_start = ip;
	while (ip[0] != '\0') {
		if (ip[0] == '.') {
			if (previous_was_dot) return false;
			count_nums = 0;
			++count_dots;
			previous_was_dot = true;
		}
		else {
			if (++count_nums > 3) return false;
			previous_was_dot = false;
		}
		++ip;
	}
	ip = ip_start;
	return (count_dots == 3);
}

struct wave_header* receive_wav_header(asp_socket* sock) {
	void* buffer = receive_packet(sock);
	asp_packet* packet = deserialize_asp(buffer);

	if (packet != NULL) {
		printf("Received a valid packet!\n");
		return deserialize_wav_header(packet->data);
	}
	else {
		printf("The packet was invalid!\n");
	}
}

uint8_t* receive_wav_samples(asp_socket* sock) {
	void* buffer = receive_packet(sock);
	asp_packet* packet = deserialize_asp(buffer);

	if (packet != NULL) {
		return packet->data;
	}
	else {
		printf("The packet was invalid!\n");
		exit(0);
	}
}

int main(int argc, char **argv) {
	unsigned int buffer_size = 1024;

	char SERVER_IP[16];	// largest length for ip: sizeof(XXX.XXX.XXX.XXX\0) == 16
	snprintf(SERVER_IP, 16, "%s", "127.0.0.1");


	uint8_t *recvbuffer;
	uint8_t *recv_ptr;
	uint8_t *playbuffer;
	uint8_t *play_ptr;

	int opt;
	while ((opt = getopt(argc, argv, "b:s:")) != -1) {
		switch (opt) {
			case 'b':
				buffer_size = atoi(optarg);
			break;
			case 's': {
				if (valid_ip(optarg)) snprintf(SERVER_IP, 16, "%s", optarg);
				else {
					printf("%s is not a valid IP address!\n", optarg);
					usage(argv[0]);
				}
				break;
			}
		default:
			usage(argv[0]);
		}
	}
	if (optind < argc) usage(argv[0]);


	/* Set up network connection */
	asp_socket sock = new_socket(1233);
	set_remote_addr(&sock, SERVER_IP, ASP_SERVER_PORT);

	char input[] = "SS";
	printf("Sending new packet!\n\tPayload: %s\n", input);
	send_asp_packet(&sock, input);

	struct wave_header* header = receive_wav_header(&sock);

	printf("%s:%d is streaming a wav file with ",
			inet_ntoa(sock.info.remote_addr.sin_addr), ntohs(sock.info.remote_addr.sin_port));
	printf("mode %s, samplerate %lu\n",
				header->n_channels == 2 ? "Stereo" : "Mono",
				header->n_samples_per_sec);

	/* Open audio device */
	snd_pcm_t *snd_handle;

	int err = snd_pcm_open(&snd_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);

	if (err < 0) {
		fprintf(stderr, "couldnt open audio device: %s\n", snd_strerror(err));
		return -1;
	}

	/* Configure parameters of PCM output */
	err = snd_pcm_set_params(snd_handle,
								SND_PCM_FORMAT_S16_LE,
								SND_PCM_ACCESS_RW_INTERLEAVED,
								header->n_channels,
								header->n_samples_per_sec,
								0,          /* Allow software resampling */
								500000);    /* 0.5 seconds latency */
	if (err < 0) {
		printf("couldnt configure audio device: %s\n", snd_strerror(err));
		return -1;
	}

	/* set up buffers/queues */
	recvbuffer = malloc(buffer_size);
	playbuffer = malloc(buffer_size);

	/* TODO: fill the buffer */
	memcpy(playbuffer, receive_wav_samples(&sock), buffer_size);
	//memcpy(playbuffer, recvbuffer, buffer_size);

	/* Play */
	printf("playing...\n");

	int i = 0;
	recv_ptr = recvbuffer;
	while (1) {
		if (i <= 0) {
			play_ptr = playbuffer;
			i = buffer_size;
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

		if (i <= 0) {
			memcpy(playbuffer, recvbuffer, buffer_size);
			memcpy(recvbuffer, receive_wav_samples(&sock), buffer_size);
		}
	}

	/* clean up */
	free(recvbuffer);
	free(playbuffer);

	snd_pcm_drain(snd_handle);
	snd_pcm_hw_free(snd_handle);
	snd_pcm_close(snd_handle);

	return 0;
}