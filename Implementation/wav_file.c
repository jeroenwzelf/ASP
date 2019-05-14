#include "wav_file.h"

int open_wave_file(struct wave_file *wf, const char *filename) {
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
	if (wf->payload_size <= 0) {
		fprintf(stderr, "couldn't find sample data of %s\n", filename);
		return -1;
	}

	printf("file %s, mode %s, samplerate %lu, time %.1f sec\n",
				filename, wf->wh->n_channels == 2 ? "Stereo" : "Mono",
				wf->wh->n_samples_per_sec, playlength);

	return 0;
}

void close_wave_file(struct wave_file *wf) {
	munmap(wf->data, wf->data_size);
	close(wf->fd);
}