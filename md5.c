// http://www.ioccc.org/2020/kurdyukov1/prog.orig.c
// The 27th IOCCC best utility isn't *that* obfuscated.
// Try 2015/hou next time, maybe?

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

const unsigned char *md5(FILE *fp) {
	static uint32_t k[64];
	if (!*k) for (int i = 0; i < 64; i++) k[i] = ldexp(fabs(sin(i + 1)), 32);

	uint32_t a = 0x67452301, b = 0xefcdab89, c = ~a, d = ~b;
	uint32_t hash[4] = {a, b, c, d}, s[16] = {0};
	size_t n = 0;
	const long start = ftell(fp);
	for (;;) {
		int ch = feof(fp) ? 0 : fgetc(fp);
		if (ch == EOF) ch = 0x80;
		s[n / 4] = (s[n / 4] >> 8) | (ch << 24);
		n++;
		bool done = n == 56 && feof(fp);
		if (done) {
			long size = ftell(fp) - start;
			s[14] = size << 3;
			s[15] = size >> 29;
			n = 64;
		}
		if (n == 64) {
			for (size_t i = 0; i < 64; i++) {
				uint32_t x = a + s[("AECG"[i / 16] * i + "0150"[i / 16]) & 15] + k[i];
				switch (i / 16) {
				case 0: x += ((c ^ d) & b) ^ d; break;
				case 1: x += ((b ^ c) & d) ^ c; break;
				case 2: x += b ^ c ^ d; break;
				case 3: x += (~d | b) ^ c; break;
				}
				a = d;
				d = c;
				c = b;
				n = "GLQVEINTDKPWFJOU"[(i >> 4 << 2) | (i & 3)] % 32;
				b += (x << n) | (x >> (32 - n));
			}
			a = hash[0] += a;
			b = hash[1] += b;
			c = hash[2] += c;
			d = hash[3] += d;
			n = 0;
		}
		if (done) break;
	}

	static unsigned char ret[16];
	for (size_t i = 0; i < 16; i++) {
		ret[i] = hash[i / 4] >> (i % 4 * 8);
	}
	return ret;
}
//
int main() {
	#define testmd5(str, ...) do {\
		FILE *f = tmpfile(); \
		fputs(str, f); \
		rewind(f); \
		if (memcmp(md5(f), (const unsigned char []) {__VA_ARGS__}, 16) != 0) \
			printf("MD5 test failure on line %d with \"%s\"\n", __LINE__, str); \
		fclose(f); \
	} while (0);
	testmd5("",
		0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04, 0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e);
	testmd5("a",
		0x0c, 0xc1, 0x75, 0xb9, 0xc0, 0xf1, 0xb6, 0xa8, 0x31, 0xc3, 0x99, 0xe2, 0x69, 0x77, 0x26, 0x61);
	testmd5("abc",
		0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0, 0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72);
	testmd5("message digest",
		0xf9, 0x6b, 0x69, 0x7d, 0x7c, 0xb7, 0x93, 0x8d, 0x52, 0x5a, 0x2f, 0x31, 0xaa, 0xf1, 0x61, 0xd0);
	testmd5("abcdefghijklmnopqrstuvwxyz",
		0xc3, 0xfc, 0xd3, 0xd7, 0x61, 0x92, 0xe4, 0x00, 0x7d, 0xfb, 0x49, 0x6c, 0xca, 0x67, 0xe1, 0x3b);
	testmd5("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
		0xd1, 0x74, 0xab, 0x98, 0xd2, 0x77, 0xd9, 0xf5, 0xa5, 0x61, 0x1c, 0x2c, 0x9f, 0x41, 0x9d, 0x9f);
	testmd5("12345678901234567890123456789012345678901234567890123456789012345678901234567890",
		0x57, 0xed, 0xf4, 0xa2, 0x2b, 0xe3, 0xc9, 0x55, 0xac, 0x49, 0xda, 0x2e, 0x21, 0x07, 0xb6, 0x7a);
}
