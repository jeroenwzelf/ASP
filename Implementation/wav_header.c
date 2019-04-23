#include "wav_header.h"

uint16_t sizeof_wav_header() {
	return
		  (4 * sizeof(char))
		+ sizeof(uint32_t)
		+ (4 * sizeof(char))
		+ (4 * sizeof(char))
		+ sizeof(uint32_t)
		+ sizeof(uint16_t)
		+ sizeof(uint16_t)
		+ sizeof(uint32_t)
		+ sizeof(uint32_t)
		+ sizeof(uint16_t)
		+ sizeof(uint16_t);
}

void print_header(struct wave_header* header) {
	printf("WAVE HEADER {\n");
	printf("\tchar riff_id[4]:\t %.*s\n", 4, header->riff_id);
	printf("\tuint32_t size:\t%i\n", header->size);
	printf("\tchar wave_id[4]:\t %.*s\n", 4, header->wave_id);
	printf("\tchar format_id[4]:\t %.*s\n", 4, header->format_id);
	printf("\tuint32_t format_size:\t%i\n", header->format_size);
	printf("\tuint16_t w_format_tag:\t%i\n", header->w_format_tag);
	printf("\tuint16_t n_channels:\t%i\n", header->n_channels);
	printf("\tuint32_t n_samples_per_sec:\t%i\n", header->n_samples_per_sec);
	printf("\tuint32_t n_avg_bytes_per_sec:\t%i\n", header->n_avg_bytes_per_sec);
	printf("\tuint16_t n_block_align:\t%i\n", header->n_block_align);
	printf("\tuint16_t w_bits_per_sample:\t%i\n", header->w_bits_per_sample);
	printf("}\n");
}

void* serialize_wav_header(struct wave_header* header) {
	void* buffer = malloc(sizeof_wav_header(header));
	void* buffer_start = buffer;

	// Convert header from host to network
	uint32_t size = htonl(header->size);
	uint32_t format_size = htonl(header->format_size);
	uint16_t w_format_tag = htons(header->w_format_tag);
	uint16_t n_channels = htons(header->n_channels);
	uint32_t n_samples_per_sec = htonl(header->n_samples_per_sec);
	uint32_t n_avg_bytes_per_sec = htonl(header->n_avg_bytes_per_sec);
	uint16_t n_block_align = htons(header->n_block_align);
	uint16_t w_bits_per_sample = htons(header->w_bits_per_sample);

	// Write struct to buffer
	memcpy(buffer, header->riff_id, 4 * sizeof(char));
	buffer += 4 * sizeof(char);

	memcpy(buffer, &size, sizeof(uint32_t)); 
	buffer += sizeof(uint32_t);

	memcpy(buffer, header->wave_id, 4 * sizeof(char));
	buffer += 4 * sizeof(char);

	memcpy(buffer, header->format_id, 4 * sizeof(char));
	buffer += 4 * sizeof(char);	

	memcpy(buffer, &format_size, sizeof(uint32_t)); 
	buffer += sizeof(uint32_t);

	memcpy(buffer, &w_format_tag, sizeof(uint16_t)); 
	buffer += sizeof(uint16_t);

	memcpy(buffer, &n_channels, sizeof(uint16_t)); 
	buffer += sizeof(uint16_t);

	memcpy(buffer, &n_samples_per_sec, sizeof(uint32_t)); 
	buffer += sizeof(uint32_t);

	memcpy(buffer, &n_avg_bytes_per_sec, sizeof(uint32_t)); 
	buffer += sizeof(uint32_t);

	memcpy(buffer, &n_block_align, sizeof(uint16_t)); 
	buffer += sizeof(uint16_t);

	memcpy(buffer, &w_bits_per_sample, sizeof(uint16_t)); 
	buffer += sizeof(uint16_t);

	print_header(header);

	return buffer_start;
}

struct wave_header* deserialize_wav_header(void* buffer) {
	struct wave_header* header = malloc(sizeof_wav_header());


	memcpy(header->riff_id, buffer, 4 * sizeof(char));
	buffer += 4 * sizeof(char);

	header->size = ntohl(* (uint32_t*) buffer);
	buffer += sizeof(uint32_t);

	memcpy(header->wave_id, buffer, 4 * sizeof(char));
	buffer += 4 * sizeof(char);

	memcpy(header->format_id, buffer, 4 * sizeof(char));
	buffer += 4 * sizeof(char);	

	header->format_size = ntohl(* (uint32_t*) buffer);
	buffer += sizeof(uint32_t);	

	header->w_format_tag = ntohs(* (uint16_t*) buffer);
	buffer += sizeof(uint16_t);

	header->n_channels = ntohs(* (uint16_t*) buffer);
	buffer += sizeof(uint16_t);

	header->n_samples_per_sec = ntohl(* (uint32_t*) buffer);
	buffer += sizeof(uint32_t);	

	header->n_avg_bytes_per_sec = ntohl(* (uint32_t*) buffer);
	buffer += sizeof(uint32_t);	

	header->n_block_align = ntohs(* (uint16_t*) buffer);
	buffer += sizeof(uint16_t);

	header->w_bits_per_sample = ntohs(* (uint16_t*) buffer);
	buffer += sizeof(uint16_t);

	print_header(header);

	return header;
}