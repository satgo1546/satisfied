// https://www.hanshq.net/zip.html
// https://justanapplication.wordpress.com/android-internals/

// https://android.googlesource.com/platform/frameworks/base/+/master/libs/androidfw/include/androidfw/ResourceTypes.h
// https://android.googlesource.com/platform/frameworks/base/+/master/libs/androidfw/ResourceTypes.cpp
// The above files are placed accordingly as of Android S.
// They have long existed before Android 1.6, but the directory structure has changed much and may still change in the future.

// aapt dump xmltree a.apk AndroidManifest.xml
// aapt dump xmlstrings a.apk AndroidManifest.xml

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "bits.c"
#define main mian
#include "mpn.c"
#undef main

// UTF8_FLAG gets introduced in Android 2.2.
// utf8_to_utf16_length is used to get the “length” of a string in ResourceTypes.cpp.
// The “length” field is therefore in UTF-16 codes (i.e., codepoints outside BMP count as two) instead of codepoints.
size_t utf8_utf16_length(const char *s) {
	size_t r = 0;
	while (*s) {
		r += ((*s & 0xc0) != 0x80) + ((unsigned char) *s >= 0xf0);
		s++;
	}
	return r;
}

// This function does not check for encoding errors and buffer overflow.
void utf8_to_utf16(uint16_t *dest, const char *src) {
	uint32_t x;
	while ((x = (unsigned char) *src++)) {
		uint32_t n = 0;
		while (x & (0x80 >> n)) n++;
		if (n) {
			// Read the following continuation bytes.
			x &= 0x7f >> n;
			while (--n) {
				x <<= 6;
				x |= (unsigned char) *src++ & 63;
			}
		}
		if (x & 0xff0000) {
			// Write a UTF-16 surrogate pair.
			x -= 0x10000;
			*dest++ = 0xd800 | (x >> 10);
			*dest++ = 0xdc00 | (x & 0x3ff);
		} else {
			*dest++ = x;
		}
	}
}

size_t base64_btoa(void *dest, const void *src, size_t size) {
	static const char base64_table[64] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char *a = dest;
	const uint8_t *b = src;
	uint32_t w = 0;
	for (size_t i = 0; i < size; i++) {
		w <<= 8;
		w |= b[i];
		if (i % 3 == 2) {
			*a++ = base64_table[w >> 18];
			*a++ = base64_table[(w >> 12) & 0x3f];
			*a++ = base64_table[(w >> 6) & 0x3f];
			*a++ = base64_table[w & 0x3f];
			w = 0;
		}
	}
	if (size % 3 == 1) {
		*a++ = base64_table[w >> 2];
		*a++ = base64_table[(w << 4) & 0x3f];
		*a++ = '=';
		*a++ = '=';
	} else if (size % 3 == 2) {
		*a++ = base64_table[w >> 10];
		*a++ = base64_table[(w >> 4) & 0x3f];
		*a++ = base64_table[(w << 2) & 0x3f];
		*a++ = '=';
	}
	return a - (char *) dest;
}

size_t base64_atob(void *dest, const void *src, size_t size) {
	unsigned char *b = dest;
	const char *a = src;
	uint32_t w = 0;
	size_t count = 0;
	for (size_t i = 0; i < size; i++) {
		uint32_t t;
		if (a[i] >= 'A' && a[i] <= 'Z') {
			t = a[i] - 'A';
		} else if (a[i] >= 'a' && a[i] <= 'z') {
			t = a[i] - 'a' + 26;
		} else if (a[i] >= '0' && a[i] <= '9') {
			t = a[i] - '0' + 52;
		} else if (a[i] == '+') {
			t = 62;
		} else if (a[i] == '/') {
			t = 63;
		} else if (a[i] == '=') {
			break;
		} else {
			// Ignore unknown characters.
			continue;
		}
		w <<= 6;
		w |= t;
		count++;
		if (count % 4 == 0) {
			*b++ = w >> 16;
			*b++ = w >> 8;
			*b++ = w;
			w = 0;
		}
	}
	switch (count % 4) {
	case 1:
		abort();
	case 2:
		*b++ = w >> 4;
		break;
	case 3:
		*b++ = w >> 10;
		*b++ = w >> 2;
		break;
	}
	return b - (unsigned char *) dest;
}

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

unsigned char *sha1(FILE *fp) {
	uint32_t a = 0x67452301, b = 0xefcdab89, c = 0x98badcfe, d = 0x10325476, e = 0xc3d2e1f0;
	uint32_t hash[5] = {a, b, c, d, e}, w[80] = {0};
	size_t n = 0;
	const long start = ftell(fp);
	for (;;) {
		int ch = feof(fp) ? 0 : fgetc(fp);
		if (ch == EOF) ch = 0x80;
		w[n / 4] = (w[n / 4] << 8) | ch;
		n++;
		bool done = n == 56 && feof(fp);
		if (done) {
			long size = ftell(fp) - start;
			w[14] = size >> 29;
			w[15] = size << 3;
			n = 64;
		}
		if (n == 64) {
			for (size_t i = 16; i < 80; i++) {
				w[i] = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
				w[i] = (w[i] << 1) | (w[i] >> 31);
			}
			for (size_t i = 0; i < 80; i++) {
				uint32_t x = e + ((a << 5) | (a >> 27)) + w[i];
				switch (i / 20) {
				case 0: x += 0x5a827999 + (d ^ (b & (c ^ d))); break;
				case 1: x += 0x6ed9eba1 + (b ^ c ^ d); break;
				case 2: x += 0x8f1bbcdc + ((b & c) | (d & (b | c))); break;
				case 3: x += 0xca62c1d6 + (b ^ c ^ d); break;
				}
				e = d;
				d = c;
				c = (b << 30) | (b >> 2);
				b = a;
				a = x;
			}
			a = hash[0] += a;
			b = hash[1] += b;
			c = hash[2] += c;
			d = hash[3] += d;
			e = hash[4] += e;
			n = 0;
		}
		if (done) break;
	}

	static unsigned char ret[20];
	for (int i = 0; i < 20; i++) {
		ret[i] = hash[i / 4] >> ((3 - i % 4) * 8);
	}
	return ret;
}

// Generate the signature according to RSASSA-PKCS1-V1_5-SIGN with SHA-1.
// 2048-bit keys are the only supported.
void mpn_sign_pkcs1_sha1(struct mpn *m, const unsigned char *sha1, const struct mpn *modulus, const struct mpn *d) {
	// https://datatracker.ietf.org/doc/html/rfc8017#section-8.2.1
	m->d[5] = 0x05000414;
	m->d[6] = 0x0e03021a;
	m->d[7] = 0x0906052b;
	m->d[8] = 0x00302130;
	m->d[63] = 0x0001ffff;
	m->count = 64;
	for (size_t i = 0; i < 5; i++) {
		size_t j = 16 - i * 4;
		m->d[i] = (sha1[j] << 24) | (sha1[j + 1] << 16) | (sha1[j + 2] << 8) | sha1[j + 3];
	}
	for (size_t i = 9; i < 63; i++) m->d[i] = 0xffffffff;
	mpn_powmod(m, d, modulus);
}



// https://letsencrypt.org/docs/a-warm-welcome-to-asn1-and-der/

// This is a very inefficient implementation of a generic ASN.1 DER writer.
// Unlike ZIP, which kindly puts the central directory at the end, ASN.1 DER doesn't seem to be optimized for writing.
struct der_file {
	FILE *fp;
	// The stack records positions where pending objects start their values in the file.
	// An object not completely written yet has a temporary structure in the file as follows.
	//   tag length          value
	//   [xx][00][00][00][00][xx][xx]…
	//                        ↑ An item on the stack points here.
	// Tags longer than one byte are unsupported.
	long stack[32];
	size_t stack_top;
};

void der_begin_file_fp(struct der_file *this, FILE *fp) {
	this->fp = fp;
	this->stack_top = 0;
}

void der_end_file(struct der_file *this) {
	assert(!this->stack_top);
	write_eof(this->fp);
}

static size_t der_write_length(FILE *fp, size_t x) {
	assert(x < 0x1000000);
	if (x < 0x80) {
		write8(fp, x);
		return 1;
	} else if (x < 0x100) {
		fputc(0x81, fp);
		write8(fp, x);
		return 2;
	} else if (x < 0x10000) {
		fputc(0x82, fp);
		write16be(fp, x);
		return 3;
	} else {
		fputc(0x83, fp);
		write8(fp, x >> 16);
		write16be(fp, x);
		return 4;
	}
}

// It is sometimes convenient to have a zero prepended, in BIT STRINGs used as OCTET STRINGs, in unsigned integers with the MSB set, etc.
void der_add(struct der_file *this, uint8_t tag, const void *buffer, size_t size, bool prepend_nul) {
	fputc(tag, this->fp);
	der_write_length(this->fp, size + prepend_nul);
	if (prepend_nul) fputc(0, this->fp);
	fwrite(buffer, 1, size, this->fp);
}

// Begin a field whose size is indeterminate.
// A matching call to der_end will adjust the length, causing everything inside the field to shift.
void der_begin(struct der_file *this, uint8_t tag) {
	if (this->stack_top >= 32) fprintf(stderr, "%s\n", __func__), exit(EXIT_FAILURE);
	fputc(tag, this->fp); // type
	write32(this->fp, 0); // length (to be calculated)
	this->stack[this->stack_top++] = ftell(this->fp);
	(void) this->fp; // value (to be written)
}

void der_end(struct der_file *this) {
	assert(this->stack_top);
	long size = ftell(this->fp) - this->stack[this->stack_top - 1];
	fseek(this->fp, this->stack[this->stack_top - 1] - 4, SEEK_SET); // length
	snap_file(this->fp, 4 - der_write_length(this->fp, size));
	this->stack_top--;
	fseek(this->fp, 0, SEEK_END);
}

void der_add_null(struct der_file *this) {
	fwrite("\5", 1, 2, this->fp);
}

void der_add_bool(struct der_file *this, bool x) {
	fprintf(this->fp, "\x01\x01%c", -x);
}

//   der_add_oid(this, 1, 2, 840, 113549, 1, 1, 11, -1);
// inserts 1.2.840.113549.1.1.11 (sha256WithRSAEncryption).
void der_add_oid(struct der_file *this, int x, int y, ...) {
	va_list args;
	va_start(args, y);
	der_begin(this, 0x06); // OBJECT IDENTIFIER
		write8(this->fp, x * 40 + y);
		while ((x = va_arg(args, int)) >= 0) {
			if (x >= 0x10000000) write8(this->fp, (x >> 28) | 0x80);
			if (x >= 0x200000) write8(this->fp, (x >> 21) | 0x80);
			if (x >= 0x4000) write8(this->fp, (x >> 14) | 0x80);
			if (x >= 0x80) write8(this->fp, (x >> 7) | 0x80);
			write8(this->fp, x & 0x7f);
		}
	der_end(this);
	va_end(args);
}

void der_add_int32(struct der_file *this, int32_t x) {
	if (x >= -0x80 && x < 0x80) {
		fprintf(this->fp, "\x02\x01%c", x);
	} else if (x >= -0x8000 && x < 0x8000) {
		fprintf(this->fp, "\x02\x02%c%c", x >> 8, x);
	} else if (x >= -0x800000 && x < 0x800000) {
		fprintf(this->fp, "\x02\x03%c%c%c", x >> 16, x >> 8, x);
	} else {
		fputc(0x02, this->fp);
		fputc(0x04, this->fp);
		write32be(this->fp, x);
	}
}

void der_add_uint32(struct der_file *this, uint32_t x) {
	// The final 0xff is there to ensure a non-empty encoding of zero.
	unsigned char a[8] = {0, 0, 0, x >> 24, x >> 16, x >> 8, x, 0xff};
	unsigned char *p = a;
	while (!p[1] && !(p[2] & 0x80)) p++;
	p[-1] = 0x02;
	*p = a + 6 - p;
	a[7] = 0;
	fwrite(p - 1, 1, *p + 2, this->fp);
}

void der_add_mpn(struct der_file *this, const struct mpn *a) {
	if (a->count <= 1) {
		der_add_uint32(this, a->count ? a->d[0] : 0);
		return;
	}
	int bits = mpn_count_bits(a);
	size_t size = (a->count - 1) * 4 + (bits + 7) / 8;
	unsigned char b[size];
	for (size_t i = size - 1; i != SIZE_MAX; i--) {
		b[size - 1 - i] = a->d[i / 4] >> (i % 4 * 8);
	}
	der_add(this, 0x02, b, size, !(bits % 8));
}

void der_add_string(struct der_file *this, uint8_t tag, const char *str) {
	der_add(this, tag, str, strlen(str), false);
}

void der_add_time(struct der_file *this, time_t t, bool force_generalized) {
	struct tm *tm = gmtime(&t);
	char buffer[18];
	assert(strftime(
		buffer,
		sizeof(buffer),
		force_generalized || tm->tm_year < 50 || tm->tm_year >= 150
			? "\x18\x0f%Y%m%d%H%M%SZ"
			: "\x17\x0d%y%m%d%H%M%SZ",
		tm
	));
	fputs(buffer, this->fp);
}

// DN = X.520 distinguished name
void der_add_dn_for_android_testkey(struct der_file *this) {
	// Is CN=Unknown, OU=Unknown, O=Unknown, L=Unknown, ST=Unknown, C=Unknown correct?
	//   [no]:  y
	der_begin(this, 0x30); // SEQUENCE
		der_begin(this, 0x31); // SET
			der_begin(this, 0x30); // SEQUENCE
				der_add_oid(this, 2, 5, 4, 6, -1); // countryName
				der_add_string(this, 0x13, "US"); // PrintableString
			der_end(this);
		der_end(this);
		der_begin(this, 0x31); // SET
			der_begin(this, 0x30); // SEQUENCE
				der_add_oid(this, 2, 5, 4, 8, -1); // stateOrProvinceName
				der_add_string(this, 0x13, "California"); // PrintableString
			der_end(this);
		der_end(this);
		der_begin(this, 0x31); // SET
			der_begin(this, 0x30); // SEQUENCE
				der_add_oid(this, 2, 5, 4, 7, -1); // localityName
				der_add_string(this, 0x13, "Mountain View"); // PrintableString
			der_end(this);
		der_end(this);
		der_begin(this, 0x31); // SET
			der_begin(this, 0x30); // SEQUENCE
				der_add_oid(this, 2, 5, 4, 10, -1); // organizationName
				der_add_string(this, 0x13, "Android"); // PrintableString
			der_end(this);
		der_end(this);
		der_begin(this, 0x31); // SET
			der_begin(this, 0x30); // SEQUENCE
				der_add_oid(this, 2, 5, 4, 11, -1); // organizationalUnitName
				der_add_string(this, 0x13, "Android"); // PrintableString
			der_end(this);
		der_end(this);
		der_begin(this, 0x31); // SET
			der_begin(this, 0x30); // SEQUENCE
				der_add_oid(this, 2, 5, 4, 3, -1); // commonName
				der_add_string(this, 0x13, "Android"); // PrintableString
			der_end(this);
		der_end(this);
		der_begin(this, 0x31); // SET
			der_begin(this, 0x30); // SEQUENCE
				der_add_oid(this, 1, 2, 840, 113549, 1, 9, 1, -1); // emailAddress
				der_add_string(this, 0x16, "android@android.com"); // IA5String
			der_end(this);
		der_end(this);
	der_end(this);
}



// The following private key “testkey.pk8” is public.
// https://android.googlesource.com/platform/build/+/refs/heads/master/target/product/security/
const struct {
	struct mpn modulus, e, d;
} apk_testkey = {
	.modulus = {{
		0x6afee91f, 0x7fa31d5b, 0x38a0b217, 0x99df9bae,
		0xfe72991d, 0x727d3c04, 0x20943f99, 0xd08e7826,
		0x69e7c8a2, 0xdeeccc8e, 0x6b9af76f, 0x553311c4,
		0x07b9e247, 0x54c8bbca, 0x6a540d81, 0x48dbf567,
		0x98c92877, 0x134fbfde, 0x01b32564, 0x24581948,
		0x6cddc3b8, 0x0cd444da, 0xfe0381cc, 0xf15818df,
		0xc06e6d42, 0x2e2f6412, 0x093a6737, 0x94d83b31,
		0xa466c87a, 0xb3f284a0, 0xa694ec2c, 0x053359e6,
		0x9717ee6a, 0x0732e080, 0x220d5008, 0xdc4af350,
		0x93d0a7c3, 0xe330c9ea, 0xcac3da1e, 0x8ebecf8f,
		0xc2be387f, 0x38a14e89, 0x211586f0, 0x18b846f5,
		0x43be4c72, 0xb578c204, 0x1bbfb230, 0xf1e267a8,
		0xa2d3e656, 0x64b8e4fe, 0xe7e83d4b, 0x3e77a943,
		0x3559ffd9, 0x0ebb0f99, 0x0aa76ce6, 0xd3786ea7,
		0xbca8cd6b, 0x068ca8e8, 0xeb1de2ff, 0x3e3ecd6c,
		0xe0d9d825, 0xb1edc762, 0xdec60b24, 0xd6931904,
	}, 64},
	.e = {{3}, 1},
	.d = {{
		0x4ca1847b, 0xcbde96bd, 0x07dc4776, 0x080c48a4,
		0x14017896, 0x49e717a2, 0x4a1bca01, 0xc5ea64ef,
		0x2b52c1f9, 0xfa25943f, 0x664ac998, 0x93a413ab,
		0x441e6fab, 0x95579d70, 0x7ee2c37e, 0xe8fd476b,
		0x64671a8e, 0x3b9cda4c, 0xebaa7c75, 0x77d2654c,
		0x99a7fc89, 0xfc6ad27b, 0x1be56997, 0xac12291c,
		0xf213a657, 0x93a9bf10, 0xfaf538a4, 0xda7564ad,
		0x4e5836a1, 0xb40276e4, 0x8e35c533, 0x75a28679,
		0xba0ff445, 0xaf774055, 0xc15e355a, 0xe831f78a,
		0xb7e06fd7, 0x9775dbf1, 0x872d3c14, 0xb47f350a,
		0xd7297aff, 0x7b16345b, 0x160e59f5, 0x65d02f4e,
		0x827edda1, 0x23a5d6ad, 0xbd2a76cb, 0x4bec451a,
		0xc1e2998f, 0xedd098a9, 0xeff028dc, 0x7efa70d7,
		0x2391553b, 0x09d20a66, 0xb1c4f344, 0xe25049c4,
		0x7dc5de47, 0x045dc5f0, 0xf213ecaa, 0x297f339d,
		0xeb3be56e, 0xcbf3da41, 0x9484076d, 0x8f0cbb58,
	}, 64},
};

struct zip_archive {
	FILE *fp;
	FILE *jar_manifest;
	FILE *jar_signature;
	struct {
		long lfh_start;
		long contents_start;
		bool binary;
		uint32_t external_file_attributes;
	} files[100];
	size_t file_count;
	bool file_began;
	bool jar_manifest_include_next_file;
	long jar_manifest_entry_start;
};

void zip_begin_archive_fp(struct zip_archive *this, FILE *fp, const char *mode) {
	assert(mode);
	this->fp = fp;
	this->jar_manifest = NULL;
	this->file_count = 0;
	this->file_began = false;
	while (*mode) switch (*mode++) {
	case 'J':
		this->jar_signature = tmpfile();
		// fall through
	case 'j':
		this->jar_manifest = tmpfile();
		this->jar_manifest_include_next_file = true;
		break;
	}
	if (this->jar_manifest) {
		fputs(
			"Manifest-Version: 1.0\r\n" // Consider using \n.
			"Created-By: 1.0 (Android)\r\n" // This line seems arbitrary.
			"\r\n", this->jar_manifest
		);
	}
}

void zip_begin_archive(struct zip_archive *this, const char *filename, const char *mode) {
	FILE *fp = fopen(filename, "wb+");
	if (!fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	zip_begin_archive_fp(this, fp, mode);
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
	if (this->jar_manifest) {
		if (strncmp(filename, "META-INF/", 9) == 0) {
			this->jar_manifest_include_next_file = false;
		}
		if (this->jar_manifest_include_next_file) {
			this->jar_manifest_entry_start = ftell(this->jar_manifest);
			if (fprintf(this->jar_manifest, "Name: %.66s", filename) == 72) {
				const char *next = filename + 66;
				while (fprintf(this->jar_manifest, "\r\n %.71s", next) == 74) next += 71;
			}
		}
	}
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
	if (this->jar_manifest) {
		if (this->jar_manifest_include_next_file) {
			fseek(this->fp, this->files[this->file_count - 1].contents_start, SEEK_SET);
			char buffer[28];
			base64_btoa(buffer, sha1(this->fp), 20);
			fprintf(this->jar_manifest, "\r\nSHA1-Digest: %.28s\r\n\r\n", buffer);
			fseek(this->jar_manifest, this->jar_manifest_entry_start, SEEK_SET);
			base64_btoa(buffer, sha1(this->jar_manifest), 20);
			// Copy the entry from MANIFEST.MF to A.SF because “The major part of the [signature] file is similar to the manifest file.”
			fseek(this->jar_manifest, this->jar_manifest_entry_start, SEEK_SET);
			copy_file_fp(this->jar_signature, this->jar_manifest);
			fseek(this->jar_signature, -32, SEEK_CUR);
			fwrite(buffer, 1, sizeof(buffer), this->jar_signature);
			fseek(this->jar_signature, 0, SEEK_END);
		}
		this->jar_manifest_include_next_file = true;
	}
	fseek(this->fp, 0, SEEK_END);
}

void zip_end_archive(struct zip_archive *this) {
	assert(!this->file_began);
	if (this->jar_manifest) {
		zip_begin_file(this, "META-INF/MANIFEST.MF", "", 4); // Does 1 work?
			rewind(this->jar_manifest);
			copy_file_fp(this->fp, this->jar_manifest);
		zip_end_file(this);
	}
	if (this->jar_signature) {
		// The JAR file specification says that “Each individual entry must contain at least the digest of its corresponding entry in the manifest file.” What's the “corresponding entry”? By trial and error, I conclude that it refers to "Name: …\nSHA1-Digest: …\n\n".
		zip_begin_file(this, "META-INF/A.SF", "", 4);
			rewind(this->jar_manifest);
			char buffer[28];
			base64_btoa(buffer, sha1(this->jar_manifest), 20);
			fprintf(this->fp,
				"Signature-Version: 1.0\r\n"
				"Created-By: 1.0 (Android)\r\n"
				"SHA1-Digest-Manifest: %.28s\r\n"
				// "X-Android-APK-Signed: 2\r\n"
				"\r\n",
				buffer
			);
			rewind(this->jar_signature);
			copy_file_fp(this->fp, this->jar_signature);
			fclose(this->jar_signature);
			fseek(this->fp, this->files[this->file_count - 1].contents_start, SEEK_SET);
			unsigned char signature_digest[20];
			memcpy(signature_digest, sha1(this->fp), sizeof(signature_digest));
		zip_end_file(this);
		// SHA1withRSA is the only in-JAR signing method supported since Android 1.0. The other viable choice is SHA1withDSA, which is added in 2.3.
		//   addSupportedSigAlg(OID_DIGEST_SHA1, OID_SIG_SHA1_WITH_RSA, InclusiveIntRange.from(0));
		// https://android.googlesource.com/platform/tools/apksig/+/refs/heads/pie-release/src/main/java/com/android/apksig/internal/apk/v1/V1SchemeVerifier.java
		zip_begin_file(this, "META-INF/A.RSA", "b", 4);
		{
			// https://www.cem.me/pki
			struct der_file c;
			struct mpn m;
			der_begin_file_fp(&c, this->fp);
				der_begin(&c, 0x30); // SEQUENCE
					der_add_oid(&c, 1, 2, 840, 113549, 1, 7, 2, -1); // signedData
					der_begin(&c, 0xa0); // context specific 0
						der_begin(&c, 0x30); // SEQUENCE
							der_add_uint32(&c, 1);
							der_begin(&c, 0x31); // SET
								der_begin(&c, 0x30); // SEQUENCE
									der_add_oid(&c, 1, 3, 14, 3, 2, 26, -1); // sha1
									der_add_null(&c);
								der_end(&c);
							der_end(&c);
							der_begin(&c, 0x30); // SEQUENCE
								der_add_oid(&c, 1, 2, 840, 113549, 1, 7, 1, -1); // data
							der_end(&c);
							der_begin(&c, 0xa0); // context specific 0
								der_begin(&c, 0x30); // SEQUENCE
									long start = ftell(c.fp);
									der_begin(&c, 0x30); // SEQUENCE
										der_begin(&c, 0xa0); // context specific 0
											der_add_uint32(&c, 2);
										der_end(&c);
										der_add(&c, 0x02, (const unsigned char [9]) {0x00, 0x93, 0x6e, 0xac, 0xbe, 0x07, 0xf2, 0x01, 0xdf}, 9, false);
										der_begin(&c, 0x30); // SEQUENCE
											der_add_oid(&c, 1, 2, 840, 113549, 1, 1, 5, -1); // sha1WithRSAEncryption
											der_add_null(&c);
										der_end(&c);
										der_add_dn_for_android_testkey(&c);
										der_begin(&c, 0x30); // SEQUENCE
											der_add_time(&c, 1204248826, false); // 2008-02-29T01:33:46Z
											der_add_time(&c, 2068248826, false); // 2035-07-17T01:33:46Z
										der_end(&c);
										der_add_dn_for_android_testkey(&c);
										der_begin(&c, 0x30); // SEQUENCE
											der_begin(&c, 0x30); // SEQUENCE
												der_add_oid(&c, 1, 2, 840, 113549, 1, 1, 1, -1); // rsaEncryption
												der_add_null(&c);
											der_end(&c);
											der_begin(&c, 0x03); // BIT STRING
												fputc(0, c.fp);
												der_begin(&c, 0x30); // SEQUENCE
													der_add_mpn(&c, &apk_testkey.modulus);
													der_add_mpn(&c, &apk_testkey.e);
												der_end(&c);
											der_end(&c);
										der_end(&c);
										der_begin(&c, 0xa3); // context specific 3
											der_begin(&c, 0x30); // SEQUENCE
												der_begin(&c, 0x30); // SEQUENCE
													der_add_oid(&c, 2, 5, 29, 14, -1); // subjectKeyIdentifier
													der_begin(&c, 0x04); // OCTET STRING
														der_add(&c, 0x04, (const unsigned char [20]) {0x48, 0x59, 0x00, 0x56, 0x3D, 0x27, 0x2C, 0x46, 0xAE, 0x11, 0x86, 0x05, 0xA4, 0x74, 0x19, 0xAC, 0x09, 0xCA, 0x8C, 0x11}, 20, false);
													der_end(&c);
												der_end(&c);
												der_begin(&c, 0x30); // SEQUENCE
													der_add_oid(&c, 2, 5, 29, 35, -1); // authorityKeyIdentifier
													der_begin(&c, 0x04); // OCTET STRING
														der_begin(&c, 0x30); // SEQUENCE
															der_add(&c, 0x80, (const unsigned char [20]) {0x48, 0x59, 0x00, 0x56, 0x3D, 0x27, 0x2C, 0x46, 0xAE, 0x11, 0x86, 0x05, 0xA4, 0x74, 0x19, 0xAC, 0x09, 0xCA, 0x8C, 0x11}, 20, false); // context specific 0
															der_begin(&c, 0xa1); // context specific 1
																der_begin(&c, 0xa4); // context specific 4
																	der_add_dn_for_android_testkey(&c);
																der_end(&c);
															der_end(&c);
															der_add(&c, 0x82, (const unsigned char [8]) {0x93, 0x6e, 0xac, 0xbe, 0x07, 0xf2, 0x01, 0xdf}, 8, true); // context specific 2
														der_end(&c);
													der_end(&c);
												der_end(&c);
												der_begin(&c, 0x30); // SEQUENCE
													der_add_oid(&c, 2, 5, 29, 19, -1); // basicConstraints
													der_begin(&c, 0x04); // OCTET STRING
														der_begin(&c, 0x30); // SEQUENCE
															der_add_bool(&c, true);
														der_end(&c);
													der_end(&c);
												der_end(&c);
											der_end(&c);
										der_end(&c);
									der_end(&c);
									fseek(c.fp, start, SEEK_SET);
									mpn_sign_pkcs1_sha1(&m, sha1(c.fp), &apk_testkey.modulus, &apk_testkey.d);
									der_begin(&c, 0x30); // SEQUENCE
										der_add_oid(&c, 1, 2, 840, 113549, 1, 1, 5, -1); // sha1WithRSAEncryption
										der_add_null(&c);
									der_end(&c);
									der_begin(&c, 0x03); // BIT STRING
										fputc(0, c.fp);
										for (size_t i = 63; i != SIZE_MAX; i--) write32be(c.fp, m.d[i]);
									der_end(&c);
								der_end(&c);
							der_end(&c);
							der_begin(&c, 0x31); // SET
								der_begin(&c, 0x30); // SEQUENCE
									der_add_uint32(&c, 1);
									der_begin(&c, 0x30); // SEQUENCE
										der_add_dn_for_android_testkey(&c);
										der_add(&c, 0x02, (const unsigned char [9]) {0x00, 0x93, 0x6e, 0xac, 0xbe, 0x07, 0xf2, 0x01, 0xdf}, 9, false);
									der_end(&c);
									der_begin(&c, 0x30); // SEQUENCE
										der_add_oid(&c, 1, 3, 14, 3, 2, 26, -1); // sha1
										der_add_null(&c);
									der_end(&c);
									der_begin(&c, 0x30); // SEQUENCE
										der_add_oid(&c, 1, 2, 840, 113549, 1, 1, 1, -1); // rsaEncryption
										der_add_null(&c);
									der_end(&c);
									der_begin(&c, 0x04); // OCTET STRING
										mpn_sign_pkcs1_sha1(&m, signature_digest, &apk_testkey.modulus, &apk_testkey.d);
										for (size_t i = 63; i != SIZE_MAX; i--) write32be(c.fp, m.d[i]);
									der_end(&c);
								der_end(&c);
							der_end(&c);
						der_end(&c);
					der_end(&c);
				der_end(&c);
			der_end_file(&c);
		}
		zip_end_file(this);
	}
	if (this->jar_manifest) fclose(this->jar_manifest);
	long cd_start = ftell(this->fp);
	for (size_t i = 0; i < this->file_count; i++) {
		long lfh_size = this->files[i].contents_start - this->files[i].lfh_start;
		fseek(this->fp, this->files[i].lfh_start, SEEK_SET);
		unsigned char buffer[600];
		fread(buffer, lfh_size, 1, this->fp);
		fseek(this->fp, 0, SEEK_END);
		fputs("PK\1\2", this->fp);
		write16(this->fp, (0 << 8) | 0); // version made by = MS-DOS 0.0
		fwrite(buffer + 4, 1, 26, this->fp);
		write16(this->fp, 0); // file comment length
		write16(this->fp, 0); // disk number start
		write16(this->fp, !this->files[i].binary); // internal file attributes
		write32(this->fp, this->files[i].external_file_attributes); // external file attributes
		write32(this->fp, this->files[i].lfh_start); // relative offset of local header
		fwrite(buffer + 30, lfh_size - 30, 1, this->fp);
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



// arsc_string_pool implements the ResStringPool chunk in a simple-minded manner.
// With the storage allocated ahead of time, strings too long are rejected and cause panic.
// All the strings are null-terminated, laid out sequentially in memory, and encoded in UTF-8.
// The last string in the pool comes with an additional zero byte which indicates the end.
//   begin = #0      #1                            #2                         end
//   ['H']['i'][ 0 ]['H']['e']['l']['l']['o'][ 0 ]['i'][ 0 ][ 0 ][   ] … [   ]
//   styles
//   [ −1 ][  2 ][  0 ][  0 ][  2 ][  2 ][  3 ][ −1 ][ −1 ][    ] …
// The example above illustrates a pool with three strings, namely "Hi", HTML "<i>H</i>e<i>ll</i>o" and "i".
// Since styling in an identifier pool is meaningless, the styleCount field in ResStringPool_header is either 0 or equal to stringCount in this implementation. The storage for styles is allocated upon the first call to arsc_append_styled.
struct arsc_string_pool {
	char *begin;
	const char *end;
	size_t count;
	uint32_t *styles;
	size_t style_word_count;
};

void arsc_begin_string_pool(struct arsc_string_pool *this) {
	this->begin = malloc(4096);
	if (!this->begin) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	*this->begin = 0;
	this->end = this->begin + 4096;
	// To ease pooling, the first string is always the empty string.
	this->count = 1;
	this->styles = NULL;
	this->style_word_count = 0;
}

size_t arsc_intern(struct arsc_string_pool *this, const char *s) {
	// getAttributeNamespaceID returns -1 if there is no namespace.
	if (!s) return SIZE_MAX;
	if (!*s) return 0;
	// Do a linear search for an existing entry.
	char *p;
	size_t n = 1;
	for (p = this->begin; *p; p += strlen(p) + 1) {
		if (strcmp(s, p) == 0) return n;
		n++;
	}
	if (p + strlen(s) + 1 >= this->end) fprintf(stderr, "%s\n", __func__), exit(EXIT_FAILURE);
	strcpy(p, s);
	p[strlen(p) + 1] = 0;
	this->count = n + 1;
	if (this->styles) {
		this->styles[this->style_word_count++] = UINT32_MAX;
	}
	return n;
}

void arsc_end_string_pool(struct arsc_string_pool *this, FILE *fp) {
	long start = ftell(fp);
	write16(fp, 0x0001); // RES_STRING_POOL_TYPE
	write16(fp, 28); // headerSize
	write32(fp, 0); // size (to be calculated)
	write32(fp, this->count); // stringCount
	if (this->styles) {
		write32(fp, this->count); // styleCount
		write32(fp, 1 << 8); // UTF8_FLAG
		write32(fp, this->count * 8 + 28); // stringsStart
		write32(fp, 0); // stylesStart (to be calculated)
		for (size_t i = 0; i < this->count * 2; i++) write32(fp, 0);
	} else {
		write32(fp, 0); // styleCount
		write32(fp, 1 << 8); // UTF8_FLAG
		write32(fp, this->count * 4 + 28); // stringsStart
		write32(fp, 0); // stylesStart
		for (size_t i = 0; i < this->count; i++) write32(fp, 0);
	}

	long pos[this->count];
	pos[0] = ftell(fp);
	write8(fp, 0);
	write8(fp, 0);
	write8(fp, 0);
	const char *p = this->begin;
	for (size_t i = 1; i < this->count; i++) {
		pos[i] = ftell(fp);
		size_t len = strlen(p);
		writelen7or15(fp, utf8_utf16_length(p));
		writelen7or15(fp, len);
		fwrite(p, 1, len + 1, fp);
		p += len + 1;
	}
	write_align_to(fp, 4);

	long size = ftell(fp) - start;
	if (this->styles) {
		for (size_t i = 0; i < this->style_word_count; i++) {
			write32(fp, this->styles[i]);
		}
		fseek(fp, start + 24, SEEK_SET);
		write32(fp, size); // stylesStart
		for (size_t i = 0; i < this->count; i++) {
			write32(fp, pos[i] - pos[0]);
		}
		uint32_t *p = this->styles;
		for (size_t i = 0; i < this->count; i++) {
			write32(fp, (p - this->styles) * 4);
			while (*p != UINT32_MAX) p++;
			p++;
		}
		// It is said that the style data section should be terminated by two more 0xffffffff's. No more words are written here; no problems are encountered so far, either.
		size += this->style_word_count * 4;
	} else {
		fseek(fp, start + 28, SEEK_SET);
		for (size_t i = 0; i < this->count; i++) {
			write32(fp, pos[i] - pos[0]);
		}
	}
	fseek(fp, start + 4, SEEK_SET);
	write32(fp, size);
	fseek(fp, 0, SEEK_END);

	free(this->begin);
	free(this->styles); // free(NULL) does nothing.
}

//   \1 = Start Of tag Header
//   \2 = Start Of tag Text
//   \3 = End Of tag Text and also the tag itself
// because writing an HTML parser for the sole purpose of generating rich text is a coder's shame.
// For instance, the markup
//   <font face="monospace" color="#663399">It is <u>true</u>.</font>
// translates to
//   "\1font;face=monospace;color=#663399\2It is \1u\2true\3.\3"
// The first four ASCII control characters are now seeing such a perfectly matched renaissance, if not a bit ugly.
// arsc_intern matches against the plain text, which should be how resources work. However, Android refuses to install the APK blatantly if (for example) the app name is associated with style data. If a true plain entry is needed, call arsc_intern first.
size_t arsc_append_styled(struct arsc_string_pool *this, const char *control) {
	if (!this->styles) {
		this->styles = malloc(1024 * 4);
		if (!this->styles) {
			perror("malloc");
			return arsc_intern(this, control);
		}
		memset(this->styles, 0xff, this->count * 4);
		this->style_word_count = this->count;
	}

	// As s is scanned, plain text is filtered in-place, as the plain text will always be shorter than the formatted control string.
	char s[strlen(control) + 1];
	strcpy(s, control);
	// \1i\2\3 is the shortest possible tag, <i></i>.
	size_t tags[(sizeof(s) - 1) / 4 + 1][3];
	size_t tag_count = 0;
	char *src = s, *dest = s;
	while (*src) switch (*src++) {
	case 1:
		{
			char *const p = strchr(src, 2);
			*p = 0;
			tags[tag_count][0] = arsc_intern(this, src); // name
			tags[tag_count][1] = dest - s; // firstChar
			tags[tag_count][2] = SIZE_MAX; // lastChar
			tag_count++;
			src = p + 1;
		}
		break;
	case 2:
		abort();
		break;
	case 3:
		for (size_t i = tag_count - 1; i != SIZE_MAX; i--) {
			if (tags[i][2] == SIZE_MAX) {
				tags[i][2] = dest - s - 1;
				break;
			}
		}
		break;
	default:
		*dest++ = src[-1];
	}
	*dest = 0;

	char *p;
	size_t n = 1;
	for (p = this->begin; *p; p += strlen(p) + 1) n++;
	strcpy(p, s);
	p[strlen(p) + 1] = 0;
	this->count = n + 1;
	for (size_t i = 0; i < tag_count; i++) {
		// ResStringPool_span
		for (size_t j = 0; j < 3; j++) {
			this->styles[this->style_word_count++] = tags[i][j];
		}
	}
	this->styles[this->style_word_count++] = UINT32_MAX;
	return n;
}



struct arsc_file {
	FILE *fp;
	uint16_t package_name[128];
	struct arsc_string_pool string_pool;
	// A string pool has to be used because names are not required to stay unique across different resource types.
	struct arsc_string_pool keys;
	uint8_t spec_type_id;
	size_t spec_entry_count;
	long spec_entry_flags_start;
	uint32_t *spec_entry_flags;
	uint32_t spec_entry_flag_mask;
	// The number of name-value pairs in the current complex resource entry. Used internally.
	size_t current_map_size;
	long config_start;
};

#define arsc_type_count 20
static_assert(arsc_type_count < 57, "arsc_type_count");
static const unsigned char arsc_types[284] = {
	0x01, 0x00, // RES_STRING_POOL_TYPE
	28, 0, // headerSize
	sizeof(arsc_types) & 0xff, sizeof(arsc_types) >> 8, // size
	[8] = arsc_type_count, // stringCount
	[16] = 0, 1, // UTF8_FLAG
	#define stringsStart (28 + arsc_type_count * 4)
	[20] = stringsStart, // stringsStart

	// The following types, except <plurals>, <menu> and <font>, are used in public.xml.
	// <array>, <integer-array> and <string-array> are collected under the same type, array.
	[28 + 0 * 4] = 0, [stringsStart + 0] = 4, 4, 'a', 't', 't', 'r', 0, // 0x01
	[28 + 1 * 4] = 7, [stringsStart + 7] = 2, 2, 'i', 'd', 0, // 0x02
	[28 + 2 * 4] = 12, [stringsStart + 12] = 5, 5, 's', 't', 'y', 'l', 'e', 0, // 0x03
	[28 + 3 * 4] = 20, [stringsStart + 20] = 6, 6, 's', 't', 'r', 'i', 'n', 'g', 0, // 0x04
	[28 + 4 * 4] = 29, [stringsStart + 29] = 5, 5, 'd', 'i', 'm', 'e', 'n', 0, // 0x05
	[28 + 5 * 4] = 37, [stringsStart + 37] = 5, 5, 'c', 'o', 'l', 'o', 'r', 0, // 0x06
	[28 + 6 * 4] = 45, [stringsStart + 45] = 5, 5, 'a', 'r', 'r', 'a', 'y', 0, // 0x07
	[28 + 7 * 4] = 53, [stringsStart + 53] = 8, 8, 'd', 'r', 'a', 'w', 'a', 'b', 'l', 'e', 0, // 0x08
	[28 + 8 * 4] = 64, [stringsStart + 64] = 6, 6, 'l', 'a', 'y', 'o', 'u', 't', 0, // 0x09
	[28 + 9 * 4] = 73, [stringsStart + 73] = 4, 4, 'a', 'n', 'i', 'm', 0, // 0x0a
	[28 + 10 * 4] = 80, [stringsStart + 80] = 8, 8, 'a', 'n', 'i', 'm', 'a', 't', 'o', 'r', 0, // 0x0b
	[28 + 11 * 4] = 91, [stringsStart + 91] = 12, 12, 'i', 'n', 't', 'e', 'r', 'p', 'o', 'l', 'a', 't', 'o', 'r', 0, // 0x0c
	[28 + 12 * 4] = 106, [stringsStart + 106] = 6, 6, 'm', 'i', 'p', 'm', 'a', 'p', 0, // 0x0d
	[28 + 13 * 4] = 115, [stringsStart + 115] = 7, 7, 'i', 'n', 't', 'e', 'g', 'e', 'r', 0, // 0x0e
	[28 + 14 * 4] = 125, [stringsStart + 125] = 10, 10, 't', 'r', 'a', 'n', 's', 'i', 't', 'i', 'o', 'n', 0, // 0x0f
	[28 + 15 * 4] = 138, [stringsStart + 138] = 3, 3, 'r', 'a', 'w', 0, // 0x10
	[28 + 16 * 4] = 144, [stringsStart + 144] = 4, 4, 'b', 'o', 'o', 'l', 0, // 0x11
	[28 + 17 * 4] = 151, [stringsStart + 151] = 7, 7, 'p', 'l', 'u', 'r', 'a', 'l', 's', 0, // 0x12
	[28 + 18 * 4] = 161, [stringsStart + 161] = 4, 4, 'm', 'e', 'n', 'u', 0, // 0x13
	[28 + 19 * 4] = 168, [stringsStart + 168] = 4, 4, 'f', 'o', 'n', 't', 0, // 0x14
	#undef stringsStart
};
static_assert(sizeof(arsc_types) < 0x10000, "arsc_types");
static_assert(sizeof(arsc_types) % 4 == 0, "arsc_types");

void arsc_begin_file(struct arsc_file *this, const char *package_name) {
	this->fp = tmpfile();
	if (!this->fp) {
		perror("tmpfile");
		exit(EXIT_FAILURE);
	}
	memset(this->package_name, 0, sizeof(this->package_name));
	utf8_to_utf16(this->package_name, package_name);
	arsc_begin_string_pool(&this->string_pool);
	arsc_begin_string_pool(&this->keys);
	this->spec_type_id = 0;
	this->current_map_size = SIZE_MAX;
	this->config_start = 0;
}

void arsc_end_file_fp(struct arsc_file *this, FILE *fp) {
	const long start = ftell(fp);
	write16(fp, 0x0002); // RES_TABLE_TYPE
	write16(fp, 12); // headerSize
	write32(fp, 0); // size (to be calculated)
	write32(fp, 1); // packageCount

	arsc_end_string_pool(&this->string_pool, fp);

	const long package_start = ftell(fp);
	write16(fp, 0x0200); // RES_TABLE_PACKAGE_TYPE
	write16(fp, 284); // headerSize
	write32(fp, 0); // size (to be calculated)
	write32(fp, 0x7f); // id
	for (size_t i = 0; i < 128; i++) write16(fp, this->package_name[i]);
	write32(fp, 284); // typeStrings
	write32(fp, arsc_type_count); // lastPublicType
	write32(fp, 284 + sizeof(arsc_types)); // keyStrings
	write32(fp, this->keys.count); // lastPublicKey

	fwrite(arsc_types, 1, sizeof(arsc_types), fp);
	arsc_end_string_pool(&this->keys, fp);

	rewind(this->fp);
	copy_file_fp(fp, this->fp);

	long size = ftell(fp) - start;
	fseek(fp, start + 4, SEEK_SET);
	write32(fp, size);
	size += start - package_start;
	fseek(fp, package_start + 4, SEEK_SET);
	write32(fp, size);
	fseek(fp, 0, SEEK_END);

	fclose(this->fp);
}

void arsc_end_file(struct arsc_file *this, const char *filename) {
	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	arsc_end_file_fp(this, fp);
	fclose(fp);
}

void arsc_begin_type(struct arsc_file *this, uint8_t type, size_t entry_count) {
	// Type IDs start	at 1; 0 is invalid.
	assert(type && type <= arsc_type_count);
	assert(!this->spec_type_id);
	write16(this->fp, 0x0202); // RES_TABLE_TYPE_SPEC_TYPE
	write16(this->fp, 16); // headerSize
	write32(this->fp, entry_count * 4 + 16); // size
	write32(this->fp, type); // id, res0, res1
	write32(this->fp, entry_count); // entryCount
	// The flags for each entry indicate what may affect its modifier resolution process.
	// They seem unused; Android chooses the right resource even if all the flags are zero.
	this->spec_entry_flags_start = ftell(this->fp);
	this->spec_entry_flags = calloc(entry_count, 4);
	if (!this->spec_entry_flags) {
		perror("calloc");
		exit(EXIT_FAILURE);
	}
	fwrite(this->spec_entry_flags, 4, entry_count, this->fp);
	this->spec_type_id = type;
	this->spec_entry_count = entry_count;
}

void arsc_end_type(struct arsc_file *this) {
	assert(this->spec_type_id);
	this->spec_type_id = 0;
	fseek(this->fp, this->spec_entry_flags_start, SEEK_SET);
	for (size_t i = 0; i < this->spec_entry_count; i++) {
		write32(this->fp, this->spec_entry_flags[i]);
	}
	free(this->spec_entry_flags);
	fseek(this->fp, 0, SEEK_END);
}

#define ARSC_CONFIGURATION_SIZE 56

void arsc_begin_configuration(struct arsc_file *this,
	uint16_t mcc, uint16_t mnc,
	const char *bcp47,
	uint16_t screen_layout,
	uint16_t smallest_width, uint16_t width, uint16_t height, uint8_t color,
	uint8_t orientation, uint8_t ui_mode, uint16_t density, uint8_t touchscreen,
	uint8_t keyboard, uint8_t navigation, uint8_t input_flags,
	uint16_t sdk
) {
	assert(this->spec_type_id);
	assert(!this->config_start);

	write16(this->fp, 0x0201); // RES_TABLE_TYPE_TYPE
	write16(this->fp, 20 + ARSC_CONFIGURATION_SIZE); // headerSize
	write32(this->fp, 0); // size (to be calculated)

	write32(this->fp, this->spec_type_id); // id, res0, res1
	write32(this->fp, this->spec_entry_count); // entryCount
	write32(this->fp, this->spec_entry_count * 4 + 20 + ARSC_CONFIGURATION_SIZE); // entriesStart

	// sizeof(ResTable_config) has once changed in 2014, but has remained constant since then.
	// https://developer.android.com/guide/topics/resources/providing-resources
	// https://android.googlesource.com/platform/frameworks/base.git/+/master/tools/aapt/AaptConfig.cpp
	this->config_start = ftell(this->fp);
	write32(this->fp, ARSC_CONFIGURATION_SIZE); // size
	write16(this->fp, mcc); // mcc (mobile country code (from SIM))
	write16(this->fp, mnc); // mnc (mobile network code (from SIM))

	// The following logic is mostly copied from AaptLocaleValue::initFromDirName, to which no significant changes have been made since 2014.
	char locale_script[4] = {0}, locale_variant[8] = {0}, locale_numbering_system[8] = {0};
	if (bcp47 && *bcp47) {
		// Partition bcp47 at '-' like strtok.
		char language[strlen(bcp47) + 1];
		strcpy(language, bcp47);
		char *subtags[6] = {language};
		size_t subtag_count = 1;
		for (char *p = language; (p = strchr(p, '-')); *p++ = 0) {
			if (subtag_count >= 6) {
				fprintf(stderr, "%s: BCP 47 tag with too many parts (%s)\n", __func__, bcp47);
				exit(EXIT_FAILURE);
			}
			subtags[subtag_count++] = p + 1;
		}
		// https://github.com/unicode-org/cldr/blob/master/common/bcp47/number.xml
		if (strcmp(subtags[subtag_count - 2], "nu") == 0) {
			strcpy(locale_numbering_system, subtags[subtag_count - 1]);
			subtag_count -= 2;
		}
		// The language is always the first subtag.
		if (subtags[0][2]) {
			write16be(this->fp,
				0x8000 | ((subtags[0][0] & 0x1f) - 1)
				| (((subtags[0][1] & 0x1f) - 1) << 5)
				| (((subtags[0][2] & 0x1f) - 1) << 10)
			); // language
		} else {
			assert(subtags[0][1]);
			write8(this->fp, tolower(subtags[0][0])); // language[0]
			write8(this->fp, tolower(subtags[0][1])); // language[1]
		}

		// Rearrange the other information so that
		//   subtags[1] = script
		//   subtags[2] = region
		//   subtags[3] = variant
		if (subtag_count == 2) {
			// The second tag can either be a region, a variant or a script.
			switch (strlen(subtags[1])) {
			case 2:
			case 3:
				subtags[2] = subtags[1];
				subtags[1] = NULL;
				break;
			case 4:
				if (isalpha(subtags[1][0]) && isalpha(subtags[1][1])
					&& isalpha(subtags[1][2]) && isalpha(subtags[1][3])) break;
				// This is not alphabetical, so we fall through to variant.
			case 5:
			case 6:
			case 7:
			case 8:
				subtags[3] = subtags[1];
				subtags[1] = NULL;
				break;
			default:
				fprintf(stderr, "%s: invalid BCP 47 tag %s\n", __func__, bcp47);
				exit(EXIT_FAILURE);
			}
		} else if (subtag_count == 3) {
			// The third tag can either be a region code (if the second tag was a script), else a variant code.
			if (strlen(subtags[2]) >= 4) {
				subtags[3] = subtags[2];
				subtags[2] = NULL;
				// The second subtag can either be a script or a region code.
				// If its size is 4, it's a script code, else it's a region code.
				if (strlen(subtags[1]) != 4) {
					subtags[2] = subtags[1];
					subtags[1] = NULL;
				}
			}
		}
		if (!subtags[2]) {
			write16(this->fp, 0); // country
		} else if (subtags[2][2]) {
			assert(isdigit(subtags[2][0]) && isdigit(subtags[2][1]) && isdigit(subtags[2][2]));
			write16be(this->fp,
				0x8000 | (subtags[2][0] & 0x0f)
				| ((subtags[2][1] & 0x0f) << 5) | ((subtags[2][2] & 0x0f) << 10)
			); // country
		} else {
			assert(subtags[2][1]);
			write8(this->fp, toupper(subtags[2][0])); // country[0]
			write8(this->fp, toupper(subtags[2][1])); // country[1]
		}
		if (subtags[1]) {
			locale_script[0] = toupper(subtags[1][0]);
			locale_script[1] = tolower(subtags[1][1]);
			locale_script[2] = tolower(subtags[1][2]);
			locale_script[3] = tolower(subtags[1][3]);
		}
		if (subtags[3]) strncpy(locale_variant, subtags[3], 8);
	} else {
		write32(this->fp, 0); // language, country
	}

	write8(this->fp, orientation); // orientation
	write8(this->fp, touchscreen); // touchscreen
	write16(this->fp, density); // density
	if (density == 0xfffe) if (sdk < 21) sdk = 21;
	write8(this->fp, keyboard); // keyboard
	write8(this->fp, navigation); // navigation
	write16(this->fp, input_flags); // inputFlags, inputPad0
	write32(this->fp, 0); // screenWidth, screenHeight
	write32(this->fp, sdk); // sdkVersion, minorVersion
	write8(this->fp, screen_layout); // screenLayout
	write8(this->fp, ui_mode); // uiMode
	write16(this->fp, smallest_width); // smallestScreenWidthDp
	write16(this->fp, width); // screenWidthDp
	write16(this->fp, height); // screenHeightDp
	fwrite(locale_script, 1, 4, this->fp); // localeScript
	fwrite(locale_variant, 1, 8, this->fp); // localeVariant
	write8(this->fp, screen_layout >> 8); // screenLayout2
	write8(this->fp, color); // colorMode
	write16(this->fp, 0); // screenConfigPad2
	// This bool field accounts for 4 bytes due to structure padding.
	write32(this->fp, false); // localeScriptWasComputed
	// I'm not sure where the following member is exactly placed, so I suppress it for now. It's added in Android Pie.
	0 && fwrite(locale_numbering_system, 1, 8, this->fp); // localeNumberingSystem
	assert(ftell(this->fp) - this->config_start == ARSC_CONFIGURATION_SIZE);

	for (size_t i = 0; i < this->spec_entry_count; i++) write32(this->fp, -1);

	this->spec_entry_flag_mask = 0;
	if (mcc) this->spec_entry_flag_mask |= 1 << 0; // CONFIG_MCC
	if (mnc) this->spec_entry_flag_mask |= 1 << 1; // CONFIG_MNC
	if (bcp47 && *bcp47) this->spec_entry_flag_mask |= 1 << 2; // CONFIG_LOCALE
	if (touchscreen) this->spec_entry_flag_mask |= 1 << 3; // CONFIG_TOUCHSCREEN
	if (keyboard) this->spec_entry_flag_mask |= 1 << 4; // CONFIG_KEYBOARD
	if (input_flags & 0x0f) this->spec_entry_flag_mask |= 1 << 5; // CONFIG_KEYBOARD_HIDDEN
	if (navigation) this->spec_entry_flag_mask |= 1 << 6; // CONFIG_NAVIGATION
	if (orientation) this->spec_entry_flag_mask |= 1 << 7; // CONFIG_ORIENTATION
	if (density) this->spec_entry_flag_mask |= 1 << 8; // CONFIG_DENSITY
	if (width || height) this->spec_entry_flag_mask |= 1 << 9; // CONFIG_SCREEN_SIZE
	if (sdk) this->spec_entry_flag_mask |= 1 << 10; // CONFIG_VERSION
	if (screen_layout & 0x003f) this->spec_entry_flag_mask |= 1 << 11; // CONFIG_SCREEN_LAYOUT
	if (ui_mode) this->spec_entry_flag_mask |= 1 << 12; // CONFIG_UI_MODE
	if (smallest_width) this->spec_entry_flag_mask |= 1 << 13; // CONFIG_SMALLEST_SCREEN_SIZE
	if (screen_layout & 0x00c0) this->spec_entry_flag_mask |= 1 << 14; // CONFIG_LAYOUTDIR
	if (screen_layout & 0x0300) this->spec_entry_flag_mask |= 1 << 15; // CONFIG_SCREEN_ROUND
	if (color & 0x0f) this->spec_entry_flag_mask |= 1 << 16; // CONFIG_COLOR_MODE
}

void arsc_end_configuration(struct arsc_file *this) {
	assert(this->config_start);
	if ((size_t) ftell(this->fp) == this->config_start + this->spec_entry_count * 4 + ARSC_CONFIGURATION_SIZE) {
		// The Android platform dislikes configurations with no entries, saying “ResTable_type entriesStart at 0x… extends beyond chunk end 0x….”
		// Setting entriesStart to 0 in this case would result in [INSTALL_PARSE_FAILED_NOT_APK: Failed to parse ….apk] on one of my devices.
		write32(this->fp, 0);
	}
	long size = ftell(this->fp) - this->config_start + 20;
	fseek(this->fp, this->config_start - 16, SEEK_SET);
	write32(this->fp, size); // size
	fseek(this->fp, 0, SEEK_END);
	this->config_start = 0;
}

static void arsc_index_entry(struct arsc_file *this, size_t index) {
	assert(this->config_start);
	assert(index < this->spec_entry_count);
	const long offset = ftell(this->fp) - this->spec_entry_count * 4 - this->config_start - ARSC_CONFIGURATION_SIZE;
	fseek(this->fp, this->config_start + index * 4 + ARSC_CONFIGURATION_SIZE, SEEK_SET);
	write32(this->fp, offset);
	fseek(this->fp, 0, SEEK_END);
	this->spec_entry_flags[index] |= this->spec_entry_flag_mask;
}

// Simple entry
void arsc_entry(struct arsc_file *this, size_t index) {
	arsc_index_entry(this, index);
	// ResTable_entry
	write16(this->fp, 8); // size
	write16(this->fp, 0); // flags
	this->current_map_size = SIZE_MAX;
}

// Complex entry
void arsc_begin_entry(struct arsc_file *this, size_t index, const char *name) {
	arsc_index_entry(this, index);
	// ResTable_map_entry : public ResTable_entry
	write16(this->fp, 16); // size
	write16(this->fp, 0x0001); // FLAG_COMPLEX
	write32(this->fp, arsc_intern(&this->keys, name)); // key
	write32(this->fp, 0); // parent
	write32(this->fp, 0); // count (to be filled in)
	this->current_map_size = 0;
}

void arsc_end_entry(struct arsc_file *this) {
	assert(this->config_start);
	fseek(this->fp, -(long) this->current_map_size * 12 - 4, SEEK_CUR);
	write32(this->fp, this->current_map_size); // count
	fseek(this->fp, 0, SEEK_END);
	this->current_map_size = SIZE_MAX;
}

void arsc_set_data(struct arsc_file *this, const char *name, uint8_t type, uint32_t data) {
	if (this->current_map_size == SIZE_MAX) {
		write32(this->fp, arsc_intern(&this->keys, name)); // key in ResTable_entry
	} else {
		this->current_map_size++;
		if (name) {
			static const char names[][6] = {
				"type", "min", "max", "l10n",
				"other", "zero", "one", "two", "few", "many",
			};
			for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
				if (strcmp(names[i], name) == 0) {
					write32(this->fp, 0x01000000 | i); // name in ResTable_map
					goto write_data;
				}
			}
			// I don't know which pool, if any, the name field in ResTable_map refers to.
			abort();
		} else {
			write32(this->fp, 0x02000000 | (this->current_map_size - 1)); // name in ResTable_map
		}
	}
write_data:
	// Res_value
	write16(this->fp, 8); // size
	write8(this->fp, 0); // res0
	write8(this->fp, type); // dataType
	write32(this->fp, data); // data
}

void arsc_set_int32(struct arsc_file *this, const char *name, int32_t value) {
	arsc_set_data(this, name, 0x10, value); // TYPE_INT_DEC
}

void arsc_set_string(struct arsc_file *this, const char *name, const char *value) {
	arsc_set_data(this, name, 0x03, arsc_intern(&this->string_pool, value)); // TYPE_STRING
}



struct axml_file {
	FILE *fp;
	size_t attr_count;
	bool inside_header;
	struct arsc_string_pool string_pool;
	struct {
		long pos;
		size_t ns;
		size_t name;
		size_t attribute_count;
	} stack[16];
	size_t stack_top;
};

void axml_begin_file(struct axml_file *this) {
	this->fp = tmpfile();
	if (!this->fp) {
		perror("tmpfile");
		exit(EXIT_FAILURE);
	}
	// The resource map is not optional at all because some read attributes by ID and some by name.
	// The first empty entry in a string pool is mandatory in this implementation.
	// Its attribute ID is set arbitrarily as long as there is no conflict.
	this->attr_count = 1;
	arsc_begin_string_pool(&this->string_pool);
	this->inside_header = true;
	this->stack_top = 0;
	write32(this->fp, 0x0101fffe);
}

static void axml_end_header(struct axml_file *this) {
	if (this->inside_header) {
		this->inside_header = false;
		// Files converted from textual XML are naturally with RES_XML_START_NAMESPACE_TYPE and RES_XML_END_NAMESPACE_TYPE blocks, but they are unused in PackageParser, so we don't need them.
	}
}

void axml_end_file_fp(struct axml_file *this, FILE *fp) {
	// A valid XML document must have a root tag.
	assert(!this->inside_header);
	assert(!this->stack_top);

	const long start = ftell(fp);
	write16(fp, 0x0003); // RES_XML_TYPE
	write16(fp, 8); // headerSize
	write32(fp, 0); // size (to be calculated)

	arsc_end_string_pool(&this->string_pool, fp);

	write16(fp, 0x0180); // RES_XML_RESOURCE_MAP_TYPE
	write16(fp, 8); // headerSize
	write32(fp, this->attr_count * 4 + 8); // size
	rewind(this->fp);
	copy_file_fp(fp, this->fp);

	long size = ftell(fp) - start;
	fseek(fp, start + 4, SEEK_SET);
	write32(fp, size);
	fseek(fp, 0, SEEK_END);

	fclose(this->fp);
}

void axml_end_file(struct axml_file *this, const char *filename) {
	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	axml_end_file_fp(this, fp);
	fclose(fp);
}

void axml_define_attr(struct axml_file *this, uint32_t id, const char *name) {
	assert(this->inside_header);
	assert(name && *name);
	write32(this->fp, id);
	arsc_intern(&this->string_pool, name);
	this->attr_count++;
	assert(this->string_pool.count == this->attr_count);
}

void axml_end_begin_element(struct axml_file *this) {
	if (!this->stack_top) return;
	size_t n = this->stack[this->stack_top - 1].attribute_count;
	if (!n) return;
	fseek(this->fp, this->stack[this->stack_top - 1].pos + 4, SEEK_SET);
	write32(this->fp, n * 20 + 36);
	fseek(this->fp, this->stack[this->stack_top - 1].pos + 28, SEEK_SET);
	write16(this->fp, n);
	fseek(this->fp, 0, SEEK_END);
}

void axml_begin_element(struct axml_file *this, const char *ns, const char *name) {
	axml_end_header(this);
	axml_end_begin_element(this);
	if (this->stack_top >= 16) fprintf(stderr, "%s\n", __func__), exit(EXIT_FAILURE);
	this->stack[this->stack_top].pos = ftell(this->fp);
	this->stack[this->stack_top].ns = arsc_intern(&this->string_pool, ns);
	this->stack[this->stack_top].name = arsc_intern(&this->string_pool, name);
	this->stack[this->stack_top].attribute_count = 0;
	this->stack_top++;
	write16(this->fp, 0x0102); // RES_XML_START_ELEMENT_TYPE
	write16(this->fp, 16); // headerSize
	write32(this->fp, 36); // size
	write32(this->fp, 0); // lineNumber
	write32(this->fp, -1); // comment
	write32(this->fp, this->stack[this->stack_top - 1].ns); // ns
	write32(this->fp, this->stack[this->stack_top - 1].name); // name
	write16(this->fp, 20); // attributeStart
	write16(this->fp, 20); // attributeSize
	write16(this->fp, 0); // attributeCount
	write16(this->fp, 0); // idIndex
	write16(this->fp, 0); // classIndex
	write16(this->fp, 0); // styleIndex
}

void axml_end_element(struct axml_file *this) {
	assert(!this->inside_header);
	axml_end_begin_element(this);
	this->stack_top--;
	write16(this->fp, 0x0103); // RES_XML_END_ELEMENT_TYPE
	write16(this->fp, 16); // headerSize
	write32(this->fp, 24); // size
	write32(this->fp, 0); // lineNumber
	write32(this->fp, -1); // comment
	write32(this->fp, this->stack[this->stack_top].ns); // ns
	write32(this->fp, this->stack[this->stack_top].name); // name
}

void axml_set_attribute(struct axml_file *this, const char *ns, const char *name, const char *raw_value, uint8_t type, uint32_t data) {
	assert(!this->inside_header);
	assert(this->stack_top);
	axml_end_begin_element(this);

	this->stack[this->stack_top - 1].attribute_count++;
	// The ns field points to the URI (instead of the name) of the desired namespace.
	// aapt always shows the name whether the ns field points to the name or the URI.
	// If the field is set incorrectly in AndroidManifest.xml, one will be greeted with the error “No value supplied for <android:name>” when installing the APK.
	write32(this->fp, arsc_intern(&this->string_pool, ns)); // ns
	write32(this->fp, arsc_intern(&this->string_pool, name)); // name
	write32(this->fp, arsc_intern(&this->string_pool, raw_value)); // rawValue
	write16(this->fp, 8); // typedValue.size
	write8(this->fp, 0); // typedValue.res0
	write8(this->fp, type); // typedValue.type
	write32(this->fp, data); // typedValue.data
	long offset = 0;
	if (strcmp(name, "id") == 0) {
		offset = 30;
	} else if (strcmp(name, "class") == 0) {
		offset = 32;
	} else if (strcmp(name, "style") == 0) {
		offset = 34;
	}
	if (offset) {
		fseek(this->fp, this->stack[this->stack_top - 1].pos + offset, SEEK_SET);
		write16(this->fp, this->stack[this->stack_top - 1].attribute_count);
		fseek(this->fp, 0, SEEK_END);
	}
}

void axml_set_int32(struct axml_file *this, const char *ns, const char *name, int32_t data) {
	axml_set_attribute(this, ns, name, NULL, 0x10, data); // TYPE_INT_DEC
}

void axml_set_string(struct axml_file *this, const char *ns, const char *name, const char *data) {
	// `aapt dump xmltree` doesn't even care about the typed data if the type is TYPE_STRING.
	// It only looks for the raw value, outputting it twice.
	axml_set_attribute(this, ns, name, data, 0x03, arsc_intern(&this->string_pool, data)); // TYPE_STRING
}

void axml_set_bool(struct axml_file *this, const char *ns, const char *name, bool data) {
	// In the case of TYPE_INT_BOOLEAN, ResourceTypes.h says that “The 'data' is either 0 or 1, for input "false" or "true" respectively.”
	// Nevertheless, aapt packages true as 0xffffffff.
	axml_set_attribute(this, ns, name, NULL, 0x12, data ? 0xffffffff : 0); // TYPE_INT_BOOLEAN
}

void axml_set_reference(struct axml_file *this, const char *ns, const char *name, uint32_t data) {
	axml_set_attribute(this, ns, name, NULL, 0x01, data); // TYPE_REFERENCE
}



int main(int argc, char **argv) {
	struct zip_archive f;
	zip_begin_archive(&f, "slzapk-output.apk", "J");

	zip_begin_file(&f, "AndroidManifest.xml", "b", 4);
	{
		struct axml_file m;
		axml_begin_file(&m);
		axml_define_attr(&m, 0x0101021b, "versionCode");
		axml_define_attr(&m, 0x0101021c, "versionName");
		axml_define_attr(&m, 0x0101020c, "minSdkVersion");
		axml_define_attr(&m, 0x01010270, "targetSdkVersion");
		axml_define_attr(&m, 0x01010003, "name");
		axml_define_attr(&m, 0x01010001, "label");
		axml_define_attr(&m, 0x01010002, "icon");
		axml_define_attr(&m, 0x01010018, "authorities");
		axml_define_attr(&m, 0x0101001b, "grantUriPermissions");
		axml_define_attr(&m, 0x01010010, "exported");

		#define ANDROID_RESOURCES "http://schemas.android.com/apk/res/android"
		axml_begin_element(&m, NULL, "manifest");
			axml_set_int32(&m, ANDROID_RESOURCES, "versionCode", 1);
			axml_set_string(&m, ANDROID_RESOURCES, "versionName", "哼，哼，啊啊啊啊啊");
			axml_set_string(&m, NULL, "package", "net.hanshq.hello");
			axml_begin_element(&m, NULL, "uses-sdk");
				axml_set_int32(&m, ANDROID_RESOURCES, "minSdkVersion", 10);
				axml_set_int32(&m, ANDROID_RESOURCES, "targetSdkVersion", 29);
			axml_end_element(&m);
			axml_begin_element(&m, NULL, "uses-permission");
				axml_set_string(&m, ANDROID_RESOURCES, "name", "android.permission.REQUEST_INSTALL_PACKAGES");
			axml_end_element(&m);
			axml_begin_element(&m, NULL, "application");
				axml_set_string(&m, ANDROID_RESOURCES, "label", "我的第一个 Java 应用程序");
				axml_set_reference(&m, ANDROID_RESOURCES, "icon", 0x7f080000);
				axml_begin_element(&m, NULL, "activity");
					axml_set_string(&m, ANDROID_RESOURCES, "name", ".MainActivity");
					axml_set_bool(&m, ANDROID_RESOURCES, "exported", true);
					axml_begin_element(&m, NULL, "intent-filter");
						axml_begin_element(&m, NULL, "action");
							axml_set_string(&m, ANDROID_RESOURCES, "name", "android.intent.action.MAIN");
						axml_end_element(&m);
						axml_begin_element(&m, NULL, "category");
							axml_set_string(&m, ANDROID_RESOURCES, "name", "android.intent.category.LAUNCHER");
						axml_end_element(&m);
					axml_end_element(&m);
				axml_end_element(&m);
				axml_begin_element(&m, NULL, "provider");
					axml_set_string(&m, ANDROID_RESOURCES, "name", ".FileProvider");
					axml_set_bool(&m, ANDROID_RESOURCES, "exported", false);
					axml_set_string(&m, ANDROID_RESOURCES, "authorities", "net.hanshq.hello.FileProvider");
					axml_set_bool(&m, ANDROID_RESOURCES, "grantUriPermissions", true);
				axml_end_element(&m);
			axml_end_element(&m);
			axml_begin_element(&m, NULL, "uses-permission");
				axml_set_string(&m, ANDROID_RESOURCES, "name", "android.permission.REQUEST_INSTALL_PACKAGES");
			axml_end_element(&m);
		axml_end_element(&m);

		axml_end_file_fp(&m, f.fp);
	}
	zip_end_file(&f);

	zip_begin_file(&f, "resources.arsc", "b", 4);
	{
		struct arsc_file r;
		arsc_begin_file(&r, "net.hanshq.hello");

		arsc_begin_type(&r, 0x04, 1); // string
			arsc_begin_configuration(&r, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
				arsc_entry(&r, 0);
				arsc_set_string(&r, "", "Grass.");
			arsc_end_configuration(&r);
			arsc_begin_configuration(&r, 0, 0, "zh-CN", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
				arsc_entry(&r, 0);
				arsc_set_string(&r, "", "草。");
			arsc_end_configuration(&r);
			arsc_begin_configuration(&r, 0, 0, "ja-JP", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
				arsc_entry(&r, 0);
				arsc_set_string(&r, "", "くさ。");
			arsc_end_configuration(&r);
		arsc_end_type(&r);
		arsc_begin_type(&r, 0x07, 1); // array
			arsc_begin_configuration(&r, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
				arsc_begin_entry(&r, 0, "");
					arsc_set_int32(&r, NULL, 114);
					arsc_set_int32(&r, NULL, 514);
					arsc_set_int32(&r, NULL, -1919);
					arsc_set_int32(&r, NULL, 810);
				arsc_end_entry(&r);
			arsc_end_configuration(&r);
		arsc_end_type(&r);
		arsc_begin_type(&r, 0x08, 1); // drawable
			arsc_begin_configuration(&r, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
				arsc_entry(&r, 0);
				arsc_set_string(&r, "", "a.png");
			arsc_end_configuration(&r);
		arsc_end_type(&r);
		arsc_begin_type(&r, 0x12, 1); // plurals
			arsc_begin_configuration(&r, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
				arsc_begin_entry(&r, 0, "");
					arsc_set_string(&r, "one", "one's string");
					arsc_set_string(&r, "other", "other's string");
				arsc_end_entry(&r);
			arsc_end_configuration(&r);
		arsc_end_type(&r);

		arsc_end_file_fp(&r, f.fp);
	}
	zip_end_file(&f);

	zip_begin_file(&f, "classes.dex", "b", 4);
		copy_file(f.fp, "classes.dex");
	zip_end_file(&f);

	zip_begin_file(&f, "a.png", "b", 4);
		copy_file(f.fp, "icon.png");
	zip_end_file(&f);

	zip_begin_file(&f, "assets/small.apk", "b", 4);
		copy_file(f.fp, "small.apk");
	zip_end_file(&f);

	zip_begin_file(&f, "lib/armeabi-v7a/libsomelib.so", "b", 4);
		copy_file(f.fp, "arm.so");
	zip_end_file(&f);

	zip_begin_file(&f, "lib/x86/libsomelib.so", "b", 4);
		copy_file(f.fp, "x86.so");
	zip_end_file(&f);

	zip_end_archive(&f);
	{
		#define testsha1(str, ...) do {\
			FILE *f = tmpfile(); \
			fputs(str, f); \
			rewind(f); \
			if (memcmp(sha1(f), (const unsigned char []) {__VA_ARGS__}, 20) != 0) \
				printf("SHA-1 test failure on line %d with \"%s\"\n", __LINE__, str); \
			fclose(f); \
		} while (0);
		testsha1("abc",
			0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a,
			0xba, 0x3e, 0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c,
			0x9c, 0xd0, 0xd8, 0x9d
		);
		testsha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
			0x84, 0x98, 0x3e, 0x44, 0x1c, 0x3b, 0xd2, 0x6e,
			0xba, 0xae, 0x4a, 0xa1, 0xf9, 0x51, 0x29, 0xe5,
			0xe5, 0x46, 0x70, 0xf1
		);
		testsha1("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
			0xe1, 0x55, 0x6d, 0x14, 0x1b, 0x01, 0x58, 0xfd,
			0x06, 0xd7, 0x27, 0xd3, 0x6d, 0x05, 0x96, 0x48,
			0xe2, 0x4a, 0x16, 0x7e
		);
	}
}
