/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 */
#include "asp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>

#include <alsa/asoundlib.h>

#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100
#define BLOCK_SIZE 1024
/* 1 Frame = Stereo 16 bit = 32 bit = 4kbit */
#define FRAME_SIZE 4

void usage(char* name) {
	fprintf(stderr, "Usage: %s [-b buffer] [-s server ip-address] \n", name);
	exit(-1);
}

asp_socket create_asp_socket_client(char* ip, int port) {
	printf("Opening a new socket to %s:%i\n", ip, port);
	asp_socket sock;
	sock.state = WORKING;

	// Create new socket
	if ((sock.info.sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		invalidate_socket(&sock, CREATE_SOCKET_FAILED, strerror(errno));
		return sock;
	}

	// Set destination address
	memset((char *) &sock.info.remote_addr, 0, sizeof(sock.info.remote_addr));
	sock.info.remote_addr.sin_family = AF_INET;
	sock.info.remote_addr.sin_port = htons(port);
	if (inet_aton(ip, &sock.info.remote_addr.sin_addr) == 0) {
		invalidate_socket(&sock, SOCKET_WRITE_FAILED, strerror(errno));
		return sock;
	}

	sock.info.remote_addrlen = sizeof(sock.info.remote_addr);
	return sock;
}

void send_asp_packet(asp_socket * sock, char* message) {
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

int main(int argc, char **argv) {
	int buffer_size = 1024;
	unsigned int blocksize = 0;

	char SERVER_IP[16];	// largest length for ip: sizeof(XXX.XXX.XXX.XXX\0) == 16
	snprintf(SERVER_IP, 16, "%s", "127.0.0.1");


	uint8_t *recvbuffer;
	uint8_t *recv_ptr;
	uint8_t *playbuffer;
	uint8_t *play_ptr;

	int opt;
	while ((opt = getopt(argc, argv, "b:d:")) != -1) {
		switch (opt) {
			case 'b':
				buffer_size = atoi(optarg);
			break;
			case 's': {
				if (valid_ip(optarg)) snprintf(SERVER_IP, 16, "%s", optarg);
				else usage(argv[0]);
			}
		default:
			usage(argv[0]);
		}
	}
	if (optind < argc) usage(argv[0]);


	/* Set up network connection */
	asp_socket sock = create_asp_socket_client(SERVER_IP, ASP_SERVER_PORT);

	send_asp_packet(&sock, "This is a packet!");
	return 0;

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
								NUM_CHANNELS,
								SAMPLE_RATE,
								0,          /* Allow software resampling */
								500000);    /* 0.5 seconds latency */
	if (err < 0) {
		printf("couldnt configure audio device: %s\n", snd_strerror(err));
		return -1;
	}

	/* set up buffers/queues */
	recvbuffer = malloc(BLOCK_SIZE);
	playbuffer = malloc(BLOCK_SIZE);

	/* TODO: fill the buffer */

	/* Play */
	printf("playing...\n");

	int i = 0;
	recv_ptr = recvbuffer;
	while (1) {
		if (i <= 0) {
			/* TODO: get sample */

			play_ptr = playbuffer;
			i = blocksize;
		}

		/* write frames to ALSA */
		snd_pcm_sframes_t frames = snd_pcm_writei(snd_handle, play_ptr, (blocksize - ((int) play_ptr - (int) playbuffer)) / FRAME_SIZE);

		/* Check for errors */
		int ret = 0;
		if (frames < 0)
			ret = snd_pcm_recover(snd_handle, frames, 0);
		if (ret < 0) {
			fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %i\n", ret);
			exit(EXIT_FAILURE);
		}
		if (frames > 0 && frames < (blocksize - ((int) play_ptr - (int) playbuffer)) / FRAME_SIZE)
			printf("Short write (expected %i, wrote %li)\n",
						(int) (blocksize - ((int) play_ptr - (int) playbuffer)) / FRAME_SIZE, frames);

		/* advance pointers accordingly */
		if (frames > 0) {
			play_ptr += frames * FRAME_SIZE;
			i -= frames * FRAME_SIZE;
		}

		if ((int) play_ptr - (int) playbuffer == blocksize)
			i = 0;


		/* TODO: try to receive a block from the server? */

	}

	/* clean up */
	free(recvbuffer);
	free(playbuffer);

	snd_pcm_drain(snd_handle);
	snd_pcm_hw_free(snd_handle);
	snd_pcm_close(snd_handle);

	return 0;
}