/* TomsFastMath, a fast ISO C bignum library. -- Tom St Denis */
#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

/* Do we want timing resistant fp_exptmod() ?
 * This makes it slower but also timing invariant with respect to the exponent
 */
/* #define TFM_TIMING_RESISTANT */

/* Max size of any number in bits.  Basically the largest size you will be multiplying
 * should be half [or smaller] of FP_MAX_SIZE-four_digit
 *
 * You can externally define this or it defaults to 4096-bits [allowing multiplications upto 2048x2048 bits ]
 */
#ifndef FP_MAX_SIZE
   #define FP_MAX_SIZE           (4096+(8*DIGIT_BIT))
#endif


/* we want no asm? */
#undef TFM_X86
#undef TFM_X86_64
#undef TFM_SSE2
#undef TFM_ARM
#undef TFM_PPC32
#undef TFM_PPC64
#undef TFM_AVR32
#undef TFM_ASM

/* ECC helpers */

/* use arc4random on platforms that support it */
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    #define FP_GEN_RANDOM()    arc4random()
    #define FP_GEN_RANDOM_MAX  0xffffffff
#endif

/* use rand() as fall-back if there's no better rand function */
#ifndef FP_GEN_RANDOM
   #define FP_GEN_RANDOM()    rand()
   #define FP_GEN_RANDOM_MAX  RAND_MAX
#endif

/* some default configurations.
 */
   /* this is to make porting into LibTomCrypt easier :-) */
      typedef unsigned long long ulong64;
      typedef signed long long   long64;

   typedef unsigned int       fp_digit;
#define SIZEOF_FP_DIGIT 4
   typedef ulong64            fp_word;

/* # of digits this is */
#define DIGIT_BIT 32
#define FP_MASK    (fp_digit)(-1)
#define FP_SIZE    (FP_MAX_SIZE/DIGIT_BIT)

/* signs */
#define FP_ZPOS     0
#define FP_NEG      1

/* return codes */
#define FP_OKAY     0
#define FP_VAL      1
#define FP_MEM      2

/* equalities */
#define FP_LT        -1   /* less than */
#define FP_EQ         0   /* equal to */
#define FP_GT         1   /* greater than */

/* replies */
#define FP_YES        1   /* yes response */
#define FP_NO         0   /* no response */

/* a FP type */
typedef struct {
	fp_digit dp[FP_SIZE];
  int used,
             sign;
} fp_int;

/* functions */

/* initialize [or zero] an fp int */
#define fp_init(a)  (void)memset((a), 0, sizeof(fp_int))
#define fp_zero(a)  fp_init(a)

/* zero/even/odd ? */
#define fp_iszero(a) (((a)->used == 0) ? FP_YES : FP_NO)
#define fp_iseven(a) (((a)->used == 0 || (((a)->dp[0] & 1) == 0)) ? FP_YES : FP_NO)
#define fp_isodd(a)  (((a)->used > 0  && (((a)->dp[0] & 1) == 1)) ? FP_YES : FP_NO)

/* set to a small digit */
void fp_set(fp_int *a, fp_digit b);

/* makes a pseudo-random int of a given size */
void fp_rand(fp_int *a, int digits);

/* copy from a to b */
#define fp_copy(a, b)      (void)(((a) != (b)) && memcpy((b), (a), sizeof(fp_int)))
#define fp_init_copy(a, b) fp_copy(b, a)

/* clamp digits */
#define fp_clamp(a)   { while ((a)->used && (a)->dp[(a)->used-1] == 0) --((a)->used); if ((a)->sign != FP_ZPOS) printf("NG used=%d sign=%d @ %s:%d\n", (a)->used,(a)->sign,__FILE__,__LINE__); (a)->sign = (a)->used ? (a)->sign : FP_ZPOS; }

/* negate and absolute */
#define fp_abs(a, b)  { fp_copy(a, b); (b)->sign  = 0; }

/* right shift x digits */
void fp_rshd(fp_int *a, int x);

/* left shift x digits */
void fp_lshd(fp_int *a, int x);

/* signed comparison */
int fp_cmp(fp_int *a, fp_int *b);

/* unsigned comparison */
int fp_cmp_mag(fp_int *a, fp_int *b);

/* power of 2 operations */
void fp_div_2d(fp_int *a, int b, fp_int *c, fp_int *d);
void fp_mod_2d(fp_int *a, int b, fp_int *c);
void fp_mul_2d(fp_int *a, int b, fp_int *c);
void fp_2expt (fp_int *a, int b);
void fp_mul_2(fp_int *a, fp_int *c);
void fp_div_2(fp_int *a, fp_int *c);

/* Counts the number of lsbs which are zero before the first zero bit */
int fp_cnt_lsb(fp_int *a);

/* c = a + b */
void fp_add(fp_int *a, fp_int *b, fp_int *c);

/* c = a - b */
void fp_sub(fp_int *a, fp_int *b, fp_int *c);

/* b = a*a  */
void fp_sqr(fp_int *a, fp_int *b);

/* a/b => cb + d == a */
//void fp_div(fp_int *a, fp_int *b, fp_int *c, fp_int *d);

/* c = a mod b, 0 <= c < b  */
void fp_mod(fp_int *a, fp_int *b, fp_int *c);

/* compare against a single digit */
int fp_cmp_d(fp_int *a, fp_digit b);

/* c = a + b */
void fp_add_d(fp_int *a, fp_digit b, fp_int *c);

/* c = a - b */
void fp_sub_d(fp_int *a, fp_digit b, fp_int *c);

/* c = a * b */
void fp_mul_d(fp_int *a, fp_digit b, fp_int *c);

/* a/b => cb + d == a */
int fp_div_d(fp_int *a, fp_digit b, fp_int *c, fp_digit *d);

/* c = a mod b, 0 <= c < b  */
int fp_mod_d(fp_int *a, fp_digit b, fp_digit *c);

/* ---> number theory <--- */
/* d = a + b (mod c) */
int fp_addmod(fp_int *a, fp_int *b, fp_int *c, fp_int *d);

/* d = a - b (mod c) */
int fp_submod(fp_int *a, fp_int *b, fp_int *c, fp_int *d);

/* d = a * b (mod c) */
int fp_mulmod(fp_int *a, fp_int *b, fp_int *c, fp_int *d);

/* c = a * a (mod b) */
int fp_sqrmod(fp_int *a, fp_int *b, fp_int *c);

/* c = 1/a (mod b) */
int fp_invmod(fp_int *a, fp_int *b, fp_int *c);

/* c = (a, b) */
void fp_gcd(fp_int *a, fp_int *b, fp_int *c);

/* c = [a, b] */
void fp_lcm(fp_int *a, fp_int *b, fp_int *c);

/* setups the montgomery reduction */
int fp_montgomery_setup(fp_int *a, fp_digit *mp);

/* computes a = B**n mod b without division or multiplication useful for
 * normalizing numbers in a Montgomery system.
 */
void fp_montgomery_calc_normalization(fp_int *a, fp_int *b);

/* computes x/R == x (mod N) via Montgomery Reduction */
void fp_montgomery_reduce(fp_int *a, fp_int *m, fp_digit mp);

/* primality stuff */

/* perform a Miller-Rabin test of a to the base b and store result in "result" */
void fp_prime_miller_rabin (fp_int * a, fp_int * b, int *result);

#define FP_PRIME_SIZE      256
/* 256 trial divisions + 8 Miller-Rabins, returns FP_YES if probable prime  */
int fp_isprime(fp_int *a);
/* extended version of fp_isprime, do 't' Miller-Rabins instead of only 8 */
int fp_isprime_ex(fp_int *a, int t);

/* Primality generation flags */
#define TFM_PRIME_BBS      0x0001 /* BBS style prime */
#define TFM_PRIME_SAFE     0x0002 /* Safe prime (p-1)/2 == prime */
#define TFM_PRIME_2MSB_OFF 0x0004 /* force 2nd MSB to 0 */
#define TFM_PRIME_2MSB_ON  0x0008 /* force 2nd MSB to 1 */

/* callback for fp_prime_random, should fill dst with random bytes and return how many read [upto len] */
typedef int tfm_prime_callback(unsigned char *dst, int len, void *dat);

#define fp_prime_random(a, t, size, bbs, cb, dat) fp_prime_random_ex(a, t, ((size) * 8) + 1, (bbs==1)?TFM_PRIME_BBS:0, cb, dat)

int fp_prime_random_ex(fp_int *a, int t, int size, int flags, tfm_prime_callback cb, void *dat);

/* VARIOUS LOW LEVEL STUFFS */
void s_fp_add(fp_int *a, fp_int *b, fp_int *c);
void s_fp_sub(fp_int *a, fp_int *b, fp_int *c);

void fp_mul_comba(fp_int *A, fp_int *B, fp_int *C);
void fp_sqr_comba(fp_int *A, fp_int *B);
