#pragma once

#include "wav_header.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

struct wave_file {
		struct wave_header *wh;
		int fd;

		void *data;
		uint32_t data_size;

		uint8_t *samples;
		uint32_t payload_size;
};

int open_wave_file(struct wave_file *wf, const char *filename);
void close_wave_file(struct wave_file *wf);