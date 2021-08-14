// http://www.ioccc.org/2020/kurdyukov1/prog.orig.c
// The 27th IOCCC best utility isn't *that* obfuscated.
// Try 2015/hou next time, maybe?

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <io.h>

int main() {
	setmode(fileno(stdin), O_BINARY);
	bool f = false;
	uint32_t a = 0x67452301, b = 0xefcdab89, c = ~a, d = ~b;
	uint32_t hash[4] = {a, b, c, d}, s[16];
	uint32_t x = a, n = 0;
	int z = 0;
	uint64_t g = 0;
	static uint32_t k[64];
	if (!*k) for (int i = 0; i < 64; i++) k[i] = ldexp(fabs(sin(i + 1)), 32);
	while (!f) {
		int ch = 0;
		if (z >= 0) {
			z = getchar();
			if (z >= 0) {
				g += 8;
				ch = z;
			} else {
				ch = 0x80;
			}
		}
		s[n / 4] = x = (x >> 8) | (ch << 24);
		n++;
		if (n == 56) {
			s[14] = g;
			s[15] = g >> 32;
			f = z < 0;
		}
		if (n == 64 || (n == 56 && f)) {
			for (int i = 0; i < 64; i++) {
				const int j = i / 16;
				x = a + s[("AECG"[j] * i + "0150"[j]) & 15] + k[i];
				switch (j) {
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
	}
	for (int i = 0; i < 32; i++) {
		putchar("0123456789abcdef"[hash[i / 8] >> (i % 8 * 4 ^ 4) & 15]);
	}
	putchar('\n');
}
