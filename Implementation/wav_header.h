#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

struct wave_header {
	char riff_id[4];
	uint32_t size;
	char wave_id[4];
	char format_id[4];
	uint32_t format_size;
	uint16_t w_format_tag;
	uint16_t n_channels;
	uint32_t n_samples_per_sec;
	uint32_t n_avg_bytes_per_sec;
	uint16_t n_block_align;
	uint16_t w_bits_per_sample;
};

uint16_t sizeof_wav_header();

// Serialization for transferring a wav header over a socket
void* serialize_wav_header(struct wave_header* header);
struct wave_header* deserialize_wav_header(void* buf);