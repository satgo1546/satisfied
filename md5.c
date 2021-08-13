// http://www.ioccc.org/2020/kurdyukov1/prog.orig.c
// The 27th IOCCC best utility isn't *that* obfuscated.
// Try hou's 2015 submission next time, maybe?

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>

int main() {
	setmode(fileno(stdin), O_BINARY);
	bool f = false;
	uint32_t a = 0x67452301, b = 0xefcdab89, c = ~a, d = ~b;
	uint32_t h[8] = {a, b, c, d, 80200, 745, 108189, 38200}, s[16];
	uint32_t x = a, n = 0;
	int z = 0;
	uint64_t g = 0;
	while (!f) {
		int ch = 0;
		if (z >= 0) {
			z = getchar();
			if (z >= 0) {
				g += 8;
				ch = z;
			} else {
				ch = 128;
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
			int64_t e = 0, k = -0x10000000000LL;
			for (int i = 0; i < 64; i++) {
				uint32_t tmp = (((e * 64336 + k * 585873) >> 21) + e * 566548 + k * 882346) >> 20;
				k = (((k * 64336 - e * 585873) >> 21) + k * 566548 - e * 882346) >> 20;
				e = tmp;
				n = h[i / 16 + 4];
				x = a + s[(n / 8 % 8 * i + n % 8) & 15] + ((e >> 8) ^ (e >> 40));
				switch (i / 16) {
				case 0: x += ((c ^ d) & b) ^ d; break;
				case 1: x += ((b ^ c) & d) ^ c; break;
				case 2: x += b ^ c ^ d; break;
				case 3: x += (~d | b) ^ c; break;
				}
				n >>= i % 4 * 3 + 6;
				n &= 7;
				n += i % 4 * 6 + 2;
				a = d;
				d = c;
				c = b;
				b += (x << n) | (x >> (32 - n));
			}
			n = 0;
			a = h[0] += a;
			b = h[1] += b;
			c = h[2] += c;
			d = h[3] += d;
		}
	}
	for (int i = 0; i < 32; i++) {
		printf("%x", h[i / 8] >> (i % 8 * 4 ^ 4) & 15);
	}
	putchar('\n');
}
