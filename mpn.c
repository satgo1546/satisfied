#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

// Most of the multiprecision routines are derived (with extensive modification) from TomsFastMath, a fast ISO C bignum library by Tom St Denis, but with all the faster cases removed.
// `struct mpn` represents an unsigned integer (“MultiPrecision Natural number”).
#define MPN_SIZE 140
struct mpn {
	uint32_t d[MPN_SIZE];
	size_t count;
};

#define mpn_clamp(x) while ((x)->count && !(x)->d[(x)->count - 1]) (x)->count--
#define max(x, y) ((x) > (y) ? (x) : (y))
#define min(x, y) ((x) < (y) ? (x) : (y))

// Print a multiprecision integer as a JavaScript BigInt literal.
void mpn_print(const struct mpn *x) {
	printf("0x");
	for (size_t i = MPN_SIZE - 1; i != SIZE_MAX; i--) {
		if (i < x->count) {
			printf("%08x%c", x->d[i], i ? '_' : 'n');
		} else {
			if (x->d[i]) printf("WTF%d?", i);
		}
	}
	if (x->count > MPN_SIZE) printf("BAD COUNT %d", x->count);
	putchar('\n');
}

// Take the most significant digit and count the bits used in it.
int mpn_count_bits(const struct mpn *x) {
	int r = 0;
	for (uint32_t q = x->d[x->count - 1]; q; q >>= 1) r++;
	return r;
}

// Comparing returns sgn(a − b) ∈ {−1, 0, 1}.
int mpn_cmp(const struct mpn *a, const struct mpn *b) {
	if (a->count > b->count) return 1;
	if (a->count < b->count) return -1;
	for (size_t i = a->count - 1; i != SIZE_MAX; i--) {
		if (a->d[i] > b->d[i]) return 1;
		if (a->d[i] < b->d[i]) return -1;
	}
	return 0;
}

// a ← a + b
void mpn_add(struct mpn *a, const struct mpn *b) {
	const size_t oldused = a->count;
	if (b->count > a->count) a->count = b->count;
	uint64_t t = 0;
	for (size_t i = 0; i < a->count; i++) {
		t += (uint64_t) a->d[i] + b->d[i];
		a->d[i] = t;
		t >>= 32;
	}
	if (t && a->count < MPN_SIZE) a->d[a->count++] = t;
	for (size_t x = a->count; x < oldused; x++) a->d[x] = 0;
}

// a ← a − b
// Unsigned subtraction — a ≥ b assumed.
void mpn_sub(struct mpn *a, const struct mpn *b) {
	const size_t oldused = a->count;
	uint64_t t = 0;
	for (size_t i = 0; i < a->count; i++) {
		t = (uint64_t) a->d[i] - b->d[i] - t;
		a->d[i] = t;
		t >>= 32;
		t &= 1;
	}
	for (size_t x = a->count; x < oldused; x++) a->d[x] = 0;
	while (a->count && !a->d[a->count - 1]) a->count--;
}

// a ← a × b
void mpn_mul(struct mpn *a, const struct mpn *b) {
	struct mpn c = {.d = {0}, .count = a->count + b->count};
	if (c.count >= MPN_SIZE) c.count = MPN_SIZE - 1;

	uint32_t c0 = 0, c1 = 0, c2 = 0;
	for (size_t ix = 0; ix < c.count; ix++) {
		// Get offsets into the two integers.
		size_t ty = min(ix, b->count - 1);
		size_t tx = ix - ty;

		// iy is the number of times the loop will iterate. Essentially it's
		//   while (tx++ < a->count && ty-- >= 0) …
		const size_t iy = min(a->count - tx, ty + 1);

		c0 = c1;
		c1 = c2;
		c2 = 0;
		for (size_t iz = 0; iz < iy; iz++) {
			uint64_t t = (uint64_t) a->d[tx++] * b->d[ty--] + c0;
			c0 = t;
			t >>= 32;
			c1 = t += c1;
			c2 += t >> 32;
		}
		c.d[ix] = c0;
	}

	while (c.count && !c.d[c.count - 1]) c.count--;
	memcpy(a, &c, sizeof(struct mpn));
}

// (q, u) ← (⌊u/v⌋, u mod v)
// q can be NULL.
void mpn_div(struct mpn *u, const struct mpn *v, struct mpn *q) {
	// This is Algorithm D in TAOCP, 4.3.1, with exercise 37 integrated.
	assert(v->count);

	// u < v  ⟹  q = 0 and remainder remains in u.
	if (mpn_cmp(u, v) < 0) {
		if (q) q->count = 0;
		return;
	}

	if (q) {
		memset(q, 0, sizeof(struct mpn));
		q->count = u->count + 2;
	}
	// Exercise 37
	uint32_t v1 = v->d[v->count - 1];
	uint32_t v2 = v->count >= 2 ? v->d[v->count - 2] : 0;
	const int e = 32 - mpn_count_bits(v);
	if (e) {
		v1 <<= e;
		v1 |= v2 >> (32 - e);
		v2 <<= e;
		if (v->count >= 3) v2 |= v->d[v->count - 3] >> (32 - e);
	}

	// D2 & D7
	for (size_t j = u->count - v->count; j != SIZE_MAX; j--) {
		// Exercise 37
		uint32_t u1 = u->d[j + v->count];
		uint32_t u2 = j + v->count >= 1 ? u->d[j + v->count - 1] : 0;
		uint32_t u3 = j + v->count >= 2 ? u->d[j + v->count - 2] : 0;
		if (e) {
			u1 <<= e;
			u1 |= u2 >> (32 - e);
			u2 <<= e;
			u2 |= u3 >> (32 - e);
			u3 <<= e;
			if (j + v->count >= 3) u3 |= u->d[j + v->count - 3] >> (32 - e);
		}
		// D3
		uint64_t qhat = ((uint64_t) u1 << 32) | u2;
		uint64_t rhat = qhat % v1;
		qhat /= v1;
		while (qhat > UINT32_MAX || qhat * v2 > ((rhat << 32) | u3)) {
			qhat--;
			rhat += v1;
			if (rhat > UINT32_MAX) break;
		}
		// D4
		uint64_t t = 0;
		for (size_t i = 0; i <= v->count; i++) {
			t = u->d[i + j] - qhat * v->d[i] - t;
			u->d[i + j] = t;
			t >>= 32;
			t = (uint32_t) -t;
		}
		// D5
		if (q) q->d[j] = qhat;
		if (t) {
			// D6
			puts("WTF??");
			mpn_print(u);
			mpn_print(v);
			qhat--;
			t = 0;
			for (size_t i = 0; i < v->count; i++) {
				t += u->d[i + j] + v->d[i];
				u->d[i + j] = t;
				t >>= 32;
			}
			u->d[v->count + j] += t;
			abort();
		}
	}
	mpn_clamp(u);
	if (q) mpn_clamp(q);
}

// Montgomery reduction.
// m is the modulus.
// m′ is required to satisfy mm′ mod 2³² = −1.
void mpn_redc(struct mpn *a, const struct mpn *m, uint32_t m_prime) {
	assert(m->count <= MPN_SIZE / 2);
	for (size_t i = 0; i < m->count; i++) {
		// Zero out the i-th digit, making a divisible by 2³²⁽ⁱ⁺¹⁾ by adding 2³²ⁱ(aᵢm′ mod 2³²)m to a.
		uint32_t mu = a->d[i] * m_prime;
		uint64_t t = 0;
		for (size_t j = 0; j < m->count; j++) {
			t += (uint64_t) mu * m->d[j] + a->d[i + j];
			a->d[i + j] = t;
			t >>= 32;
		}
		for (size_t j = m->count; t; j++) {
			t += a->d[i + j];
			a->d[i + j] = t;
			t >>= 32;
		}
	}
	a->count = m->count + 1;
	memmove(a->d, a->d + m->count, a->count * 4);
	memset(a->d + a->count, 0, (MPN_SIZE - a->count) * 4);
	mpn_clamp(a);
	if (mpn_cmp(a, m) >= 0) mpn_sub(a, m);
}

// base ← base^exponent mod m
// This is the standard fast exponentiation by squaring and multiplying, vulnerable to timing attacks.
// See TAOCP, exercise 4.3.1–41 for an explanation of Montgomery's trick.
// Knuth has w₀w′ mod b = 1 with subtraction in Montgomery reduction, while here we adopt the convention of m₀m mod 2³² = −1 with addition in REDC.
void mpn_powmod(struct mpn *base, const struct mpn *exponent, const struct mpn *m) {
	// m₀ and 2³² (the base) must be coprime.
	assert(m->count && m->d[0] & 1);
	// Fast inversion mod 2ⁿ.
	//   m₀m′ ≡ 1 (mod 2ⁿ)  ⟹  m₀(m′(2 − m₀m′)) ≡ 1 (mod 2²ⁿ)
	uint32_t m_prime = m->d[0];
	m_prime += ((m->d[0] + 2) & 4) << 1; // m₀m′ mod 2⁴ = 1
	m_prime *= 2 - m->d[0] * m_prime; // m₀m′ mod 2⁸ = 1
	m_prime *= 2 - m->d[0] * m_prime; // m₀m′ mod 2¹⁶ = 1
	m_prime *= 2 - m->d[0] * m_prime; // m₀m′ mod 2³² = 1
	m_prime = -m_prime; // m₀m′ mod 2³² = −1

	// r ← 2³²ⁿ mod m
	struct mpn r = {.d = {0}, .count = m->count + 1};
	r.d[m->count] = 1;
	mpn_div(&r, m, NULL);

	// base ← (base × r) mod m
	mpn_div(base, m, NULL);
	mpn_mul(base, &r);
	mpn_div(base, m, NULL);

	// Go through the bits in the exponent, from the most significant to the least significant.
	for (size_t digidx = exponent->count * 32 + mpn_count_bits(exponent) - 1; digidx != SIZE_MAX; digidx--) {
		// Apply the square-and-multiply algorithm.
		mpn_mul(&r, &r);
		mpn_redc(&r, m, m_prime);
		if ((exponent->d[digidx / 32] >> (digidx & 31)) & 1) {
			mpn_mul(&r, base);
			mpn_redc(&r, m, m_prime);
		}
	}
	mpn_redc(&r, m, m_prime);
	memcpy(base, &r, sizeof(struct mpn));
}

int main() {
	struct mpn e = {{65537}, 1}, n = {{
		0x270ec5e9, 0x2fe44161, 0x512b81cc, 0x6f7086ce,
		0x086fdae5, 0xa8a17c97, 0x3825be3f, 0xfcbd320d,
		0x573c7ec1, 0x2e791647, 0x594d4a4f, 0xf8eee2bd,
		0x979cebf9, 0x7f71bece, 0xe2a8fa62, 0x48048aaa,
		0x59a28a92, 0x72da2e4c, 0xc8c30ecd, 0x6280002a,
		0x78bf1dfd, 0x2e6cd9ec, 0xf75caa1c, 0x7094ee1a,
		0x3dd19e88, 0xf49d8f2f, 0x3d2ec3d8, 0x8c40043e,
		0xfd4b546f, 0xec31e4b0, 0x0a9809a5, 0xce032e86,
	}, 32}, c = {{
		0xb12a5b50, 0xdbae817f, 0x6aaa7f5a, 0x4bea3271,
		0x8723e577, 0xf52cd92d, 0x099cd01e, 0x70427b63,
		0x5e0ada51, 0x3703a7b1, 0x7d6e6204, 0xaeffd27a,
		0x051e3a2d, 0xcfa90ebb, 0xb4639c2e, 0x98db51bd,
		0x3b797dce, 0x97f21519, 0xda6c1e5a, 0x09918459,
		0x1d154853, 0x8c57efa5, 0x7f8d0add, 0x45923c94,
		0x96ba3fb4, 0x77743de9, 0xe76ac194, 0x09cc8450,
		0x01aa9372, 0x0e6b06bf, 0x968a0453, 0x9ff70ea6,
	}, 32}, m = {{
		0x83d6f6bd, 0x348688af, 0x7c148d8d, 0x65c033ec,
		0xd738e6f0, 0x0fc8689c, 0xee2621ef, 0x13dc1f6f,
		0xd85ca22a, 0xe6a29acf, 0x880f4ab2, 0x2db4cdb1,
		0xf4de18a6, 0xe1d6be57, 0xe67835ad, 0x40377851,
		0xf546123b, 0x91f13fec, 0x2b71dad7, 0x68cf0edb,
		0x81e83a73, 0xd11caf9c, 0x6d8ae214, 0xa66ed7eb,
		0xce57c649, 0x6b00818c, 0x2b5879d0, 0x29099ab5,
		0x322b33c7, 0x9390e2df, 0x250f45b9, 0x39f5a911,
	}, 32};

	/* test it */
	mpn_powmod(&m, &e, &n);
	if (memcmp(&m.d, &c.d, sizeof(c.d)) != 0) {
		printf("Encrypted text not equal\n");
	}

	/* read in the parameters */
	n = (struct mpn) {{
		0x15a9773f, 0xb1162cf1, 0x395b0889, 0x4826e160,
		0xf75dfdc5, 0x425e7260, 0x97478380, 0xf73c4d53,
		0x3ca60ee3, 0x89b64692, 0x4dfeb2ee, 0xce2f89b3,
		0x3e24e073, 0x9a8a846e, 0xb4691bd0, 0x995148cd,
		0x32176795, 0x410ac045, 0xaadb0e27, 0xf750f4c3,
		0xdaf5702e, 0xcdc9a6f3, 0xda3ea3cd, 0xee2cd9d5,
		0x3034633b, 0x28d1c7ba, 0xe97864de, 0x007adfb9,
		0xf1e404a4, 0x6936916a, 0x04d31acc, 0xa7f30e2e,
	}, 32};
	struct mpn d = {{
		0xe7028739, 0x11143a36, 0x023de403, 0xafc1d88d,
		0xf72ec5f3, 0x1f8d456c, 0x09dea577, 0xafda7987,
		0x36a13914, 0x741a57ac, 0xbe34048b, 0xbf30a63c,
		0x75f3fcaa, 0x7d60a46d, 0xf5b0f21e, 0x56323cf9,
		0xc900750a, 0xa0399db5, 0xedefa514, 0xb022479e,
		0x773e3167, 0x335d5a59, 0xb7f11ac0, 0x29dade1d,
		0x4b825299, 0x4810785a, 0x5c8906eb, 0x93fe1db7,
		0x6e8ed432, 0x10d3611e, 0xc9a404d8, 0x16d166f3,
	}, 32};
	c = (struct mpn) {{
		0x4b5812dd, 0xe525c5e3, 0xa2cf56c9, 0xdba95714,
		0x9e8e8989, 0x54618aeb, 0x769d9610, 0x0241812e,
		0x0015f69d, 0x7731731c, 0x7f164c8c, 0x7f45c0a2,
		0x0b142ccd, 0x495b9b85, 0xad4b7adf, 0x812e72fa,
		0x86cac771, 0x0fd2c888, 0x0371140b, 0x7f2c550d,
		0x8113a431, 0xcb94aa39, 0xd060ec99, 0xb064f4a0,
		0xd44de62a, 0x7f677aa4, 0x1df28524, 0xcfbdb16f,
		0x0b11d819, 0xb8428bdd, 0xc32543f5, 0x7d216641,
	}, 32};
	m = (struct mpn) {{
		0xec6f62d8, 0xb7df7b10, 0xc735e1da, 0x7fba5483,
		0x67173474, 0xa0cde0d6, 0x4d9b0bcb, 0x3e9e5d0c,
		0x09785d99, 0xae2dae58, 0x0c7252d1, 0x9b648d5a,
		0x372fb658, 0x0223b582, 0xb55c3be6, 0x69f37c14,
		0xc0530eb3, 0x5a0af4be, 0x41fd10d6, 0x3ae68ebb,
		0x8fef116d, 0xb268810f, 0x883d2982, 0x357bf5a6,
		0x0bc55d61, 0xa964b251, 0xf6b60fa2, 0xf42287de,
		0xc9883bb4, 0xfd78727d, 0xb394b98f, 0x5f323bf0,
	}, 32};

	/* test it */
	mpn_powmod(&c, &d, &n);
	if (memcmp(&c.d, &m.d, sizeof(m.d)) != 0) {
		printf("Decrypted text not equal\n");
	}

	printf("Testing. Rand max = %d.\n", RAND_MAX);
	for(int j=0;j<1234;j++){
		if (j){
			m.count = rand() % 39+1;
			for (size_t i = 0; i < m.count; i++) m.d[i] = (unsigned) rand() << 18 | rand();
			for (size_t i = m.count; i < MPN_SIZE; i++) m.d[i] = 0;
			n.count = rand() % (m.count+1)+1;
			for (size_t i = 0; i < n.count; i++) n.d[i] = (unsigned) rand() << 18 | rand();
			for (size_t i = n.count; i < MPN_SIZE; i++) n.d[i] = 0;
		} else {
			m = (struct mpn) {
				.count = 4,
				.d = {0, 0, 1, 4},
			};
			n = (struct mpn) {
				.count = 3,
				.d = {8, 8, 5},
			};
		}
		memcpy(&c,&m,sizeof(struct mpn));
		mpn_div(&m,&n,&d);
		if (!j) {
			assert(m.d[2] == 3 && m.d[1] == 0x999999a1 && m.d[0] == 0x999999a8);
			assert(d.d[0] == 0xcccccccb);
		}
		mpn_mul(&n,&d);
		mpn_add(&c,&n);
		//if(!j)fp_print(&c);
	}
	printf("Done.\n");
	return 0;
}
