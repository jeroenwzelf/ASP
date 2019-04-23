/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 */

#include "asp.h"
#include "wav_header.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <stdbool.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <netinet/in.h>

/* wave file handle */
struct wave_file {
		struct wave_header *wh;
		int fd;

		void *data;
		uint32_t data_size;

		uint8_t *samples;
		uint32_t payload_size;
};

static struct wave_file wf = {0,};

/* open the wave file */
static int open_wave_file(struct wave_file *wf, const char *filename) {
	uint8_t *p;
	float playlength;
	struct stat statbuf;
	uint32_t *size;

	/* Open the file */
	wf->fd = open(filename, O_RDONLY);
	if (wf->fd < 0) {
		fprintf(stderr, "couldn't open %s\n", filename);
		return -1;
	}

	/* Get the size of the file */
	if (fstat(wf->fd, &statbuf) < 0) return -1;
	wf->data_size = statbuf.st_size;

	/* Map the file into memory */
	wf->data = mmap(0x0, wf->data_size, PROT_READ, MAP_SHARED, wf->fd, 0);
	if (wf->data == MAP_FAILED) {
		fprintf(stderr, "mmap failed\n");
		return -1;
	}

	wf->wh = wf->data;

	/* Check whether the file is a wave file */
	if (strncmp(wf->wh->riff_id, "RIFF", 4)
			|| strncmp(wf->wh->wave_id, "WAVE", 4)
			|| strncmp(wf->wh->format_id, "fmt", 3)) {
		fprintf(stderr, "%s is not a valid wave file\n", filename);
		return -1;
	}

	/* Skip to actual data fragment */
	p = wf->data + wf->wh->format_size + 16 + 4;
	size = (uint32_t *) (p + 4);
	do {
		if (strncmp(p, "data", 4))
			break;

		wf->samples = p;
		wf->payload_size = *size;
		p += 8 + *size;
	} while (strncmp(p, "data", 4) && (uint32_t) (((void *) p) - wf->data) < statbuf.st_size);

	if (wf->wh->w_bits_per_sample != 16) {
		fprintf(stderr, "can't play sample with bitsize %d\n",
						wf->wh->w_bits_per_sample);
		return -1;
	}

	playlength = (float) *size / (wf->wh->n_channels * wf->wh->n_samples_per_sec * wf->wh->w_bits_per_sample / 8);

	printf("file %s, mode %s, samplerate %lu, time %.1f sec\n",
				filename, wf->wh->n_channels == 2 ? "Stereo" : "Mono",
				wf->wh->n_samples_per_sec, playlength);

	return 0;
}

/* close the wave file/clean up */
static void close_wave_file(struct wave_file *wf) {
	munmap(wf->data, wf->data_size);
	close(wf->fd);
}

void send_asp_packet(asp_socket* sock, void* message, uint16_t message_size) {
	if (sock->state == WORKING) {
		asp_packet packet = create_asp_packet(ntohs(sock->info.local_addr.sin_port), ntohs(sock->info.remote_addr.sin_port), message, message_size);
		send_packet(sock, serialize_asp(&packet), size(&packet));
	}
	else fprintf(stderr, "Couldn't send packet: socket is invalid!\n");
}

void start_streaming(asp_socket* sock, struct wave_file *wf) {
	// First do header
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
        	// case blabla
        default:
            fprintf(stderr, "Usage: %s [file...]\n", argv[0]);
            return -1;
        }
    }

	char* filename = argv[optind];

	/* Open the WAVE file */
	if (open_wave_file(&wf, filename) < 0) {
		return -1;
	}


	/* Set up network connection (open socket) */
	asp_socket sock = new_socket(ASP_SERVER_PORT);
	
	printf("Listening for packets...\n");
	while (true) {
		if (sock.state != WORKING) {
			fprintf(stderr, "Couldn't listen to socket: socket is invalid!\n");
			return;
		}

		void* buffer = receive_packet(&sock);
		asp_packet* packet = deserialize_asp(buffer);

		if (packet != NULL) {
			printf("Received a valid packet!\n\tPayload: %s\n", packet->data);
			if (strcmp(packet->data, "SS") == 0)
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