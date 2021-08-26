#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void write8(FILE *f, uint8_t value) {
	if (fputc(value, f) == EOF) {
		perror("fputc");
		exit(1);
	}
}

void write16(FILE *f, uint16_t x) {
	write8(f, x & 0xff);
	write8(f, (x >> 8) & 0xff);
}

void write32(FILE *f, uint32_t x) {
	write8(f, x & 0xff);
	write8(f, (x >> 8) & 0xff);
	write8(f, (x >> 16) & 0xff);
	write8(f, (x >> 24) & 0xff);
}

// Write a string to file, padded to 8 bytes.
void writestr8(FILE *f, const char *str) {
	size_t len = strlen(str);
	assert(len <= 8 && "The string must fit in 8 bytes.");
	for (size_t i = 0; i < 8; i++) {
		write8(f, i < len ? str[i] : 0);
	}
}

// Write a string to file, null-terminated and padded to 20 bytes.
void writestr20(FILE *f, const char *str) {
	size_t len = strlen(str);
	assert(len <= 19 && "The string and terminator must fit in 20 bytes.");
	for (size_t i = 0; i < 20; i++) {
		write8(f, i < len ? str[i] : 0);
	}
}

void writelen7or15(FILE *f, size_t x) {
	if (x < 0x80) {
		write8(f, x);
	} else if (x < 0x8000) {
		write8(f, (x >> 8) | 0x80);
		write8(f, x);
	} else {
		fprintf(stderr, "%s\n", __func__);
		exit(EXIT_FAILURE);
	}
}

// Seek to offset in file.
void seek(FILE *f, long offset) {
	if (fseek(f, offset, SEEK_SET) == -1) {
		perror("fseek");
		exit(1);
	}
}

void write_padding_to(FILE *f, size_t alignment) {
	size_t pos = ftell(f);
	if (pos % alignment) fprintf(f, "%*c", (int) (alignment - pos % alignment), 0);
}

size_t align_to(size_t value, size_t alignment) {
	value += alignment - 1;
	value -= value % alignment;
	return value;
}

size_t read_file(const char *filename, void **ptr) {
	FILE *f = fopen(filename, "rb");
	if (!f) {
		perror("fopen");
		exit(1);
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		perror("fseek");
		exit(1);
	}
	long sz = ftell(f);
	if (sz == -1L) {
		perror("ftell");
		exit(1);
	}
	rewind(f);
	if (!(*ptr = malloc(sz))) {
		perror("malloc");
		exit(1);
	}
	if (fread(*ptr, 1, sz, f) != (unsigned) sz) {
		perror("fread");
		exit(1);
	}
	if (fclose(f) == EOF) {
		perror("fclose");
		exit(1);
	}
	return sz;
}

size_t copy_fp(FILE *fout, FILE *fin) {
	static unsigned char* buffer = NULL;
	if (!buffer) buffer = malloc(33554432);

	size_t sz = 0;
	do {
		sz += fwrite(buffer, 1, fread(buffer, 1, 33554432, fin), fout);
	} while (!feof(fin));
	return sz;
}

size_t copy_file(FILE *f, const char *filename) {
	FILE* fin = fopen(filename, "rb");
	if (!fin) {
		perror("fopen");
		exit(1);
	}
	size_t sz = copy_fp(f, fin);
	if (fclose(fin) == EOF) {
		perror("fclose");
		exit(1);
	}
	return sz;
}
