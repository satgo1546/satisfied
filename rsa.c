#include <assert.h>
#include <stdint.h>
#include <time.h>
/* TomsFastMath, a fast ISO C bignum library. -- Tom St Denis */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

#define MPN_SIZE 140
struct mpn {
	uint32_t dp[MPN_SIZE];
	int count;
};

#define mpn_clamp(a) while ((a)->count && (a)->dp[(a)->count-1] == 0) --((a)->count)
#define max(x, y) ((x) > (y) ? (x) : (y))
#define min(x, y) ((x) < (y) ? (x) : (y))

void mpn_print(const struct mpn *a) {
	printf("0x");
	for (int i = MPN_SIZE - 1; i >= a->count; i--) if (a->dp[i]) printf("WTF%d?", i);
	for (int i = a->count - 1; i >= 0; i--) printf("%08x%c", a->dp[i], i ? '_' : 'n');
	if (a->count < 0 || a->count > MPN_SIZE) printf("BAD USED %d", a->count);
	putchar('\n');
}

int mpn_count_bits(const struct mpn *a) {
	int r = 0;
	/* take the last digit and count the bits in it */
	for (uint32_t q = a->dp[a->count - 1]; q; q >>= 1) r++;
	return r;
}

int mpn_cmp(const struct mpn *a, const struct mpn *b) {
	if (a->count > b->count) return 1;
	if (a->count < b->count) return -1;
	for (int x = a->count - 1; x >= 0; x--) {
		if (a->dp[x] > b->dp[x]) return 1;
		if (a->dp[x] < b->dp[x]) return -1;
	}
	return 0;
}

void mpn_add(const struct mpn *a, const struct mpn *b, struct mpn *c) {
	const int oldused = c->count;
	c->count = max(a->count, b->count);
	uint64_t t = 0;
	for (int x = 0; x < c->count; x++) {
		t += (uint64_t) a->dp[x] + b->dp[x];
		c->dp[x] = t;
		t >>= 32;
	}
	if (t && c->count < MPN_SIZE) c->dp[c->count++] = t;
	for (int x = c->count; x < oldused; x++) c->dp[x] = 0;
	mpn_clamp(c);
}

// unsigned subtraction ||a|| >= ||b|| ALWAYS!
void mpn_sub(const struct mpn *a, const struct mpn *b, struct mpn *c) {
	const int oldused = c->count;
	c->count = a->count;
	uint64_t t = 0;
	for (int x = 0; x < c->count; x++) {
		t = (uint64_t) a->dp[x] - b->dp[x] - t;
		c->dp[x] = t;
		t >>= 32;
		t &= 1;
	}
	for (int x = c->count; x < oldused; x++) c->dp[x] = 0;
	mpn_clamp(c);
}

/* generic PxQ multiplier */
// a *= b
void mpn_mul(struct mpn *A, const struct mpn *B) {
	int old_used = A->count;
    /* pick a comba (unrolled 4/8/16/32 x or rolled) based on the size
       of the largest input.  We also want to avoid doing excess mults if the
       inputs are not close to the next power of two.  That is, for example,
       if say y=17 then we would do (32-17)^2 = 225 unneeded multiplications */
	// get size of output and trim
	int pa = A->count + B->count;
	if (pa >= MPN_SIZE) pa = MPN_SIZE - 1;

	struct mpn tmp;
	struct mpn *dst = &tmp;
	memset(dst, 0, sizeof(struct mpn));

	uint32_t c0 = 0, c1 = 0, c2 = 0;
	for (int ix = 0; ix < pa; ix++) {
		/* get offsets into the two bignums */
		int ty = min(ix, B->count - 1);
		int tx = ix - ty;

		/* this is the number of times the loop will iterate, essentially it's
			 while (tx++ < a->count && ty-- >= 0) { ... } */
		const int iy = min(A->count - tx, ty + 1);

		/* execute loop */
		c0 = c1;
		c1 = c2;
		c2 = 0;
		for (int iz = 0; iz < iy; iz++) {
			uint64_t t = (uint64_t) A->dp[tx++] * B->dp[ty--] + c0;
			c0 = t;
			t >>= 32;
			c1 = t += c1;
			c2 += t >> 32;
		}

		/* store term */
		dst->dp[ix] = c0;
	}

	dst->count = pa;
	mpn_clamp(dst);
	memcpy(A, &tmp, sizeof(struct mpn));
	for (int y = A->count; y < old_used; y++) A->dp[y] = 0;
}

void mpn_div(struct mpn *u, const struct mpn *v, struct mpn *q) {
	// This is Algorithm D in TAOCP, 4.3.1, with exercise 37 integrated.
	assert(v->count);

	/* if a < b then q=0, r = a */
	if (mpn_cmp(u, v) < 0) return;

	if (q) {
		memset(q, 0, sizeof(struct mpn));
		q->count = u->count + 2;
	}
	uint32_t v1;
	uint32_t v2 = v->count >= 2 ? v->dp[v->count - 2] : 0;
	uint32_t v3 = v->count >= 3 ? v->dp[v->count - 3] : 0;
	int e = 0;
	for (v1 = v->dp[v->count - 1]; v1 <= UINT32_MAX / 2; v1 <<= 1) e++;
	if (e) {
		v1 |= v2 >> (32 - e);
		v2 <<= e;
		v2 |= v3 >> (32 - e);
	}

	for (int j = u->count - v->count; j >= 0; j--) {
		uint32_t u1 = u->dp[j + v->count] << e, u2 = 0, u3 = 0;
//printf("%x %x %x    %x %x %x\n",v1,v2,v3,u1,u2,u3);
		if (j + v->count >= 1) {
			if (e) u1 |= u->dp[j + v->count - 1] >> (32 - e);
			u2 = u->dp[j + v->count - 1] << e;
			if (j + v->count >= 2) {
				if (e) u2 |= u->dp[j + v->count - 2] >> (32 - e);
				u3 = u->dp[j + v->count - 2] << e;
				if (j + v->count >= 3 && e) {
					u3 |= u->dp[j + v->count - 3] >> (32 - e);
				}
			}
		}
//printf("%x %x %x    %x %x %x\n",v1,v2,v3,u1,u2,u3);
		// D3
		uint64_t qhat = ((uint64_t) u1 << 32) | u2;
		uint64_t rhat = qhat % v1;
		qhat /= v1;
//printf("%d %llx %llx\n", j, qhat, rhat);
		while (qhat - 1 == UINT32_MAX || qhat * v2 > ((rhat << 32) | u3)) {
			qhat--;
			rhat += v1;
			if (rhat > UINT32_MAX) break;
		}
		// D4
		{
			struct mpn a = {0}, c = {0};
			a.count = v->count + 1;
			c.count = 1;
			for (int i = 0; i < a.count; i++) a.dp[i] = u->dp[i + j];
			c.dp[0] = qhat;
			mpn_mul(&c, v);
			// D5
			if (mpn_cmp(&a, &c) < 0) {
				// D6
puts("WTF??");
// TODO
				qhat--;
				mpn_add(&a, v, &a);
			} else {
				// D4
				mpn_sub(&a, &c, &a);
			}
			for (int i = 0; i <= v->count; i++) u->dp[i + j] = a.dp[i];
			if (q) q->dp[j] = qhat;
		}
	}
	mpn_clamp(u);
	if (q) mpn_clamp(q);
}

/* computes x/R == x (mod N) via Montgomery Reduction */
void mpn_redc(struct mpn *a, const struct mpn *m, uint32_t mp) {
	uint32_t c[MPN_SIZE], *_c, mu;
	int x;

	// bail if too large
	assert(m->count <= MPN_SIZE / 2);

	// zero the buff
	memset(c, 0, sizeof(c));
	const int pa = m->count;

	// copy the input
	const int oldused = a->count;
	for (x = 0; x < oldused; x++) c[x] = a->dp[x];

	for (x = 0; x < pa; x++) {
		uint32_t cy = 0;
		/* get Mu for this round */
		mu = c[x] * mp;
		_c = c + x;

		for (int y = 0; y < pa; y++) {
			uint64_t t = (uint64_t) mu * m->dp[y] + _c[0] + cy;
			_c[0] = t;
			cy = t >> 32;
			_c++;
		}
		while (cy) {
			uint32_t t = _c[0] += cy;
			cy = t < cy;
			_c++;
		}
	}

	/* now copy out */
	_c = c + pa;
	for (x = 0; x < pa + 1; x++) a->dp[x] = *_c++;
	for (; x < oldused; x++) a->dp[x] = 0;
	a->count = pa + 1;
	mpn_clamp(a);

	/* if A >= m then A = A - m */
	if (mpn_cmp(a, m) >= 0) mpn_sub(a, m, a);
}

// montgomery ladder based G^X mod P → Y
// Based on work by Marc Joye, Sung-Ming Yen, "The Montgomery Powering Ladder", Cryptographic Hardware and Embedded Systems, CHES 2002 */
void mpn_powmod(const struct mpn *G, const struct mpn *X, const struct mpn *P, struct mpn *Y) {
	struct mpn R[2];
	uint32_t mp;
	int bitcnt, digidx, y;

	/* now setup montgomery */
	{
		/* fast inversion mod 2**k
 * Based on the fact that
 * XA = 1 (mod 2**n)  =>  (X(2-XA)) A = 1 (mod 2**2n)
 *                    =>  2*X*A - X*X*A*A = 1
 *                    =>  2*(1) - (1)     = 1
 */
		uint32_t b = P->dp[0];

		assert(b & 1);

		uint32_t x = (((b + 2) & 4) << 1) + b; /* here x*a==1 mod 2**4 */
		x *= 2 - b * x; /* here x*a==1 mod 2**8 */
		x *= 2 - b * x; /* here x*a==1 mod 2**16 */
		x *= 2 - b * x; /* here x*a==1 mod 2**32 */

		/* rho = -1/m mod b */
		mp = ((uint64_t) 1 << 32) - x;
	}

	memset(R, 0, sizeof(R));

	/* now we need R mod m */
	// computes a = B**n mod b without division or multiplication useful for normalizing numbers in a Montgomery system.
	{
		/* how many bits of last digit does P use */
		int bits = mpn_count_bits(P);

		/* compute A = B^(n-1) * 2^(bits-1) */
		if (P->count > 1) {
			R->count = P->count;
			R->dp[P->count - 1] = (uint32_t) 1 << (bits - 1);
		} else {
			R->count = 1;
			R->dp[0] = 1;
			bits = 1;
		}

		/* now compute C = A * B mod P */
		for (int x = bits - 1; x < 32; x++) {
			// R *= 2
			uint32_t r = 0;

			/* carry */
			for (int x = 0; x < R->count; x++) {
				/* get what will be the *next* carry bit from the MSB of the current digit */
				uint32_t rr = R->dp[x] >> 31;

				/* now shift up this digit, add in the carry [from the previous] */
				R->dp[x] <<= 1;
				R->dp[x] |= r;

				/* copy the carry that would be from the source digit into the next iteration */
				r = rr;
			}

			// new leading digit?
			// add a MSB which is always 1 at this point
			if (r && R->count != MPN_SIZE - 1) R->dp[R->count++] = 1;

			if (mpn_cmp(R, P) >= 0) mpn_sub(R, P, R);
		}
	}

	/* now set R[1] to G * R[0] mod m */
	memcpy(&R[1], G, sizeof(struct mpn));
	mpn_div(&R[1], P, NULL);
  mpn_mul(&R[1], &R[0]);
  mpn_div(&R[1], P, NULL);

	/* for j = t-1 downto 0 do
        r_!k = R0*R1; r_k = r_k^2 */

	/* set initial mode and bit cnt */
	bitcnt = 1;
	uint32_t buf = 0;
	digidx = X->count - 1;

	for (;;) {
		// grab next digit as required
		if (!--bitcnt) {
			// if digidx == -1 we are out of digits so break
			if (digidx == -1) break;
			// read next digit and reset bitcnt
			buf = X->dp[digidx--];
			bitcnt = 32;
		}

		// grab the next msb from the exponent
		y = (buf >> 31) & 1;
		buf <<= 1;

		// do ops
		mpn_mul(&R[y ^ 1], &R[y]);
		mpn_redc(&R[y ^ 1], P, mp);
		mpn_mul(&R[y], &R[y]);
		mpn_redc(&R[y], P, mp);
	}

	mpn_redc(&R[0], P, mp);
	memcpy(Y, R, sizeof(struct mpn));
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
	}, 32}, e_m;

	/* test it */
	mpn_powmod(&m, &e, &n, &e_m);
	if (memcmp(&e_m.dp, &c.dp, sizeof(c.dp)) != 0) {
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
	mpn_powmod(&c, &d, &n, &e_m);
	if (memcmp(&e_m.dp, &m.dp, sizeof(m.dp)) != 0) {
		printf("Decrypted text not equal\n");
	}

	printf("Testing. Rand max = %d.\n", RAND_MAX);
	for(int j=0;j<1234;j++){
		if (j){
			m.count = rand() % 39+1;
			for (int i = 0; i < m.count; i++) m.dp[i] = (unsigned) rand() << 18 | rand();
			for (int i = m.count; i < MPN_SIZE; i++) m.dp[i] = 0;
			n.count = rand() % (m.count+1)+1;
			for (int i = 0; i < n.count; i++) n.dp[i] = (unsigned) rand() << 18 | rand();
			for (int i = n.count; i < MPN_SIZE; i++) n.dp[i] = 0;
		} else {
			m = (struct mpn) {
				.count = 4,
				.dp = {0, 0, 1, 4},
			};
			n = (struct mpn) {
				.count = 3,
				.dp = {8, 8, 5},
			};
		}
		memcpy(&c,&m,sizeof(struct mpn));
		mpn_div(&m,&n,&d);
		if (!j) {
			assert(m.dp[2] == 3 && m.dp[1] == 0x999999a1 && m.dp[0] == 0x999999a8);
			assert(d.dp[0] == 0xcccccccb);
		}
		mpn_mul(&n,&d);
		mpn_add(&c,&n,&c);
		//if(!j)fp_print(&c);
	}
	printf("Done.\n");
	return 0;
}
