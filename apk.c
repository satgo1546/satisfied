// https://www.hanshq.net/zip.html

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bits.c"

/* See Figure 14-7 in Hacker's Delight (2nd ed.), or
   crc_reflected() in https://www.zlib.net/crc_v3.txt
   or Garry S. Brown's implementation e.g. in
   https://opensource.apple.com/source/xnu/xnu-792.13.8/bsd/libkern/crc32.c */
uint32_t crc32(FILE *fp) {
	static uint32_t crc32_table[256];
	if (!*crc32_table) {
		/* Based on Figure 14-7 in Hacker's Delight (2nd ed.) Equivalent to
				cm_tab() in https://www.zlib.net/crc_v3.txt with
				cm_width = 32, cm_refin = true, cm_poly = 0xcbf43926. */
		for (uint32_t i = 0; i < 256; i++) {
			uint32_t x = i;
			for (int j = 0; j < 8; j++) {
				x = (x >> 1) ^ (0xedb88320 & -(x & 1));
			}
			crc32_table[i] = x;
		}
	}
	uint32_t r = 0xffffffff;
	for (;;) {
		int ch = fgetc(fp);
		if (ch == EOF) break;
		r = (r >> 8) ^ crc32_table[(r ^ ch) & 0xff];
	}
	return ~r;
}

struct zip_archive {
	FILE *fp;
	struct {
		long lfh_start;
		long contents_start;
		bool binary;
		uint32_t external_file_attributes;
	} files[100];
	size_t file_count;
	bool file_began;
};

void zip_begin_archive_fp(struct zip_archive *this, FILE *fp) {
	this->fp = fp;
	this->file_count = 0;
	this->file_began = false;
}

void zip_begin_archive(struct zip_archive *this, const char *filename) {
	FILE *fp = fopen(filename, "wb+");
	if (!fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	zip_begin_archive_fp(this, fp);
}

void zip_begin_file(struct zip_archive *this, const char *filename, const char *mode, size_t contents_alignment) {
	assert(!this->file_began);
	assert(mode);
	assert(contents_alignment);
	assert(filename && *filename);
	if (this->file_count >= 100) {
		fprintf(stderr, "%s: too many files\n", __func__);
		exit(EXIT_FAILURE);
	}

	this->file_began = true;
	this->files[this->file_count].binary = false;
	this->files[this->file_count].external_file_attributes = 0;
	while (*mode) switch (*mode++) {
	case 'b':
		this->files[this->file_count].binary = true;
		break;
	}

	long lfh_start = ftell(this->fp);
	long contents_start = lfh_start + 30 + strlen(filename);
	long padding = align_to(contents_start, contents_alignment) - contents_start;
	// The padding must be in the extra fields, not before local file headers, or Android stops parsing it despite it being valid ZIP and JAR.
	contents_start += padding;
	this->files[this->file_count].lfh_start = lfh_start;
	this->files[this->file_count].contents_start = contents_start;
	bool folder = strchr("/\\", filename[strlen(filename) - 1]);
	if (folder) this->files[this->file_count].external_file_attributes |= 1 << 5;

	fputs("PK\3\4", this->fp);
	write16(this->fp, folder ? 20 : 10); // version needed to extract = PKZip 2.0
	write16(this->fp, 0); // general purpose bit flag = not encrypted
	write16(this->fp, 0); // compression method = stored
	{
		time_t t = time(NULL);
		struct tm *tm = localtime(&t);
		write16(this->fp, (tm->tm_sec / 2) | (tm->tm_min << 5) | (tm->tm_hour << 11)); // last mod file time
		// Bits 0--4:  Second divided by two.  Bits 5--10: Minute.  Bits 11-15: Hour.
		write16(this->fp, tm->tm_mday | ((tm->tm_mon + 1) << 5) | ((tm->tm_year - 80) << 9)); // last mod file date
		// Bits 0--4:  Day (1--31).  Bits 5--8:  Month (1--12).  Bits 9--15: Year from 1980.
	}
	write32(this->fp, 0); // CRC32 (to be calculated)
	write32(this->fp, 0); // compressed size (to be calculated)
	write32(this->fp, 0); // uncompressed size (to be calculated)
	write16(this->fp, strlen(filename)); // filename length
	write16(this->fp, padding); // extra field length
	fputs(filename, this->fp); // filename
	while (padding--) fputc(0, this->fp); // extra field
	this->file_count++;
}

void zip_end_file(struct zip_archive *this) {
	assert(this->file_began);
	this->file_began = false;
	long size = ftell(this->fp) - this->files[this->file_count - 1].contents_start;
	fseek(this->fp, this->files[this->file_count - 1].contents_start, SEEK_SET);
	uint32_t crc = crc32(this->fp);
	fseek(this->fp, this->files[this->file_count - 1].lfh_start + 14, SEEK_SET);
	write32(this->fp, crc); // CRC32
	write32(this->fp, size); // compressed size
	write32(this->fp, size); // uncompressed size
	fseek(this->fp, 0, SEEK_END);
}

void zip_end_archive(struct zip_archive *this) {
	static unsigned char buffer[600];
	assert(!this->file_began);
	long cd_start = ftell(this->fp);
	for (size_t i = 0; i < this->file_count; i++) {
		long lfh_size = this->files[i].contents_start - this->files[i].lfh_start;
		fseek(this->fp, this->files[i].lfh_start, SEEK_SET);
		fread(buffer, 1, lfh_size, this->fp);
		fseek(this->fp, 0, SEEK_END);
		fputs("PK\1\2", this->fp);
		write16(this->fp, (0 << 8) | 0); // version made by = MS-DOS 0.0
		fwrite(buffer + 4, 1, 26, this->fp);
		write16(this->fp, 0); // file comment length
		write16(this->fp, 0); // disk number start
		write16(this->fp, !this->files[i].binary); // internal file attributes
		write32(this->fp, this->files[i].external_file_attributes); // external file attributes
		write32(this->fp, this->files[i].lfh_start); // relative offset of local header
		fwrite(buffer + 30, 1, lfh_size - 30, this->fp);
		(void) this->fp; // file comment
	}
	long cd_size = ftell(this->fp) - cd_start;
	fputs("PK\5\6", this->fp);
	write16(this->fp, 0); // number of this disk
	write16(this->fp, 0); // number of the disk with the start of the central directory
	write16(this->fp, this->file_count); // total number of entries in the central dir on this disk
	write16(this->fp, this->file_count); // total number of entries in the central dir
	write32(this->fp, cd_size); // size of the central directory
	write32(this->fp, cd_start); // offset of start of central directory with respect to the starting disk number
	write16(this->fp, 0); // zipfile comment length
	(void) this->fp; // zipfile comment
	if (fclose(this->fp) == EOF) {
		perror("fclose");
		exit(EXIT_FAILURE);
	}
}


int main(int argc, char **argv) {
	struct zip_archive f;
	zip_begin_archive(&f, "slzapk-output.apk");

	zip_begin_file(&f, "AndroidManifest.xml", "b", 4);
	zip_end_file(&f);
	zip_begin_file(&f, "resources.arsc", "b", 4);
	zip_end_file(&f);
	zip_begin_file(&f, "classes.dex", "b", 4);
	copy_file(f.fp, "bits.c");
	zip_end_file(&f);

	zip_end_archive(&f);
}
