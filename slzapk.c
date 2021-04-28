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
uint32_t crc32(FILE *f, size_t n) {
	static uint32_t crc32_table[256];
	if (!crc32_table[0]) {
		/* Based on Figure 14-7 in Hacker's Delight (2nd ed.) Equivalent to
				cm_tab() in https://www.zlib.net/crc_v3.txt with
				cm_width = 32, cm_refin = true, cm_poly = 0xcbf43926. */
		for (uint32_t idx = 0; idx < 256; idx++) {
			uint32_t crc = idx;
			uint32_t mask;
			for (int i = 0; i < 8; i++) {
				mask = -(crc & 1);
				crc = (crc >> 1) ^ (0xedb88320 & mask);
			}
			crc32_table[idx] = crc;
		}
	}
	uint32_t crc = 0xffffffff;
	while (n--) {
		crc = (crc >> 8) ^ crc32_table[(crc ^ fgetc(f)) & 0xff];
	}
	return ~crc;
}

struct {
	size_t lfh_begin;
	size_t contents_begin;
	size_t end;
	bool binary;
	uint32_t external_file_attributes;
} zip_files[100];
size_t zip_file_count;
bool zip_file_began;

void zip_begin_file(FILE *f, const char *filename, const char *mode, size_t contents_alignment) {
	assert(!zip_file_began);
	zip_file_began = true;
	assert(mode);
	while (*mode) switch (*mode++) {
	case 'b':
		zip_files[zip_file_count].binary = true;
		break;
	}

	size_t lfh_begin = ftell(f);
	size_t contents_begin = lfh_begin + 30 + strlen(filename);
	assert(contents_alignment);
	size_t padding = align_to(contents_begin, contents_alignment) - contents_begin;
	// The padding must be in the extra fields, not before local file headers, or Android stops parsing it despite it being valid ZIP and JAR.
	contents_begin += padding;
	zip_files[zip_file_count].lfh_begin = lfh_begin;
	zip_files[zip_file_count].contents_begin = contents_begin;
	write8(f, 'P');
	write8(f, 'K');
	write8(f, 3);
	write8(f, 4);
	bool folder = strchr("/\\", filename[strlen(filename) - 1]);
	if (folder) zip_files[zip_file_count].external_file_attributes |= 1 << 5;
	write16(f, folder ? 20 : 10); // version needed to extract = PKZip 2.0
	write16(f, 0); // general purpose bit flag = not encrypted
	write16(f, 0); // compression method = stored
	{
		time_t t = time(NULL);
		struct tm *tm = localtime(&t);
		write16(f, (tm->tm_sec / 2) | (tm->tm_min << 5) | (tm->tm_hour << 11));
		// Bits 0--4:  Second divided by two.  Bits 5--10: Minute.  Bits 11-15: Hour.
		write16(f, tm->tm_mday | ((tm->tm_mon + 1) << 5) | ((tm->tm_year - 80) << 9));
		// Bits 0--4:  Day (1--31).  Bits 5--8:  Month (1--12).  Bits 9--15: Year from 1980.
	}
	write32(f, 0); // CRC32
	write32(f, 0); // compressed size
	write32(f, 0); // uncompressed size
	assert(filename && *filename);
	write16(f, strlen(filename)); // filename length
	write16(f, padding); // extra field length
	fputs(filename, f); // filename
	while (padding--) fputc(0, f); // extra field
	zip_file_count++;
}

void zip_end_file(FILE *f) {
	assert(zip_file_began);
	zip_file_began = false;
	zip_files[zip_file_count - 1].end = ftell(f);
	seek(f, zip_files[zip_file_count - 1].contents_begin);
	size_t size = zip_files[zip_file_count - 1].end - zip_files[zip_file_count - 1].contents_begin;
	uint32_t crc = crc32(f, size);
	seek(f, zip_files[zip_file_count - 1].lfh_begin + 14);
	write32(f, crc); // CRC32
	write32(f, size); // compressed size
	write32(f, size); // uncompressed size
	seek(f, zip_files[zip_file_count - 1].end);
}

void zip_write_cd(FILE *f) {
	static unsigned char buffer[600];
	assert(!zip_file_began);
	size_t cd_begin = ftell(f);
	for (size_t i = 0; i < zip_file_count; i++) {
		size_t lfh_size = zip_files[i].contents_begin - zip_files[i].lfh_begin;
		seek(f, zip_files[i].lfh_begin);
		fread(buffer, 1, lfh_size, f);
		fseek(f, 0, SEEK_END);
		write8(f, 'P');
		write8(f, 'K');
		write8(f, 1);
		write8(f, 2);
		write16(f, (0 << 8) | 0); // version made by = MS-DOS 0.0
		fwrite(buffer + 4, 1, 26, f);
		write16(f, 0); // file comment length
		write16(f, 0); // disk number start
		write16(f, !zip_files[i].binary); // internal file attributes
		write32(f, zip_files[i].external_file_attributes); // external file attributes
		write32(f, zip_files[i].lfh_begin); // relative offset of local header
		fwrite(buffer + 30, 1, lfh_size - 30, f);
		(void) f; // file comment
	}
	size_t cd_end = ftell(f);
	write8(f, 'P');
	write8(f, 'K');
	write8(f, 5);
	write8(f, 6);
	write16(f, 0); // number of this disk
	write16(f, 0); // number of the disk with the start of the central directory
	write16(f, zip_file_count); // total number of entries in the central dir on this disk
	write16(f, zip_file_count); // total number of entries in the central dir
	write32(f, cd_end - cd_begin); // size of the central directory
	write32(f, cd_begin); // offset of start of central directory with respect to the starting disk number
	write16(f, 0); // zipfile comment length
	(void) f; // zipfile comment
}


int main(int argc, char **argv) {
	FILE *f;
	size_t i;
	uint8_t* classes;
	size_t classes_sz = read_file("runapk.bat", (void**) &classes);

	f = fopen("slzapk-output.apk", "wb+");
	if (!f) {
		perror("fopen");
		return 1;
	}

	void zip_fff(const char *filename) {
		char a[100];
		if (filename[strlen(filename)-1]=='/'){
			//zip_begin_file(f, filename,"b",1);
			//zip_end_file(f);
		}else{
			strcpy(a, "sto/");
			strcat(a, filename);
			zip_begin_file(f, filename,"b",4);
			copy_file(f, a);
			zip_end_file(f);
		}
	}
zip_fff("res/layout/main.xml");
zip_fff("AndroidManifest.xml");
zip_fff("resources.arsc");
zip_fff("res/drawable-hdpi/ic_launcher.png");
zip_fff("res/drawable-hdpi/logo00001.PNG");
zip_fff("res/drawable-hdpi/logo002.PNG");
zip_fff("res/drawable-hdpi/logo003.PNG");
zip_fff("res/drawable-ldpi/ic_launcher.png");
zip_fff("res/drawable-mdpi/ic_launcher.png");
zip_fff("res/drawable-xhdpi/ic_launcher.png");
zip_fff("classes.dex");
zip_fff("META-INF/MANIFEST.MF");
zip_fff("META-INF/CERT.SF");
zip_fff("META-INF/CERT.RSA");
zip_write_cd(f);

	if (fclose(f) == EOF) {
		perror("fclose");
		return 1;
	}
	return 0;
}
