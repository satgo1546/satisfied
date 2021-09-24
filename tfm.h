#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
/* Max size of any number in bits.  Basically the largest size you will be multiplying
 * should be half [or smaller] of FP_MAX_SIZE-four_digit
 *
 * You can externally define this or it defaults to 4096-bits [allowing multiplications upto 2048x2048 bits ]
 */
   #define FP_MAX_SIZE           (4096+(8*DIGIT_BIT))

/* ECC helpers */

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

/* equalities */
#define FP_LT        -1   /* less than */
#define FP_EQ         0   /* equal to */
#define FP_GT         1   /* greater than */

typedef struct {
	fp_digit dp[FP_SIZE];
  int used, sign;
} fp_int;

/* initialize [or zero] an fp int */
#define fp_init(a)  (void)memset((a), 0, sizeof(fp_int))
#define fp_zero(a)  fp_init(a)

/* copy from a to b */
#define fp_copy(a, b)      (void)(((a) != (b)) && memcpy((b), (a), sizeof(fp_int)))
#define fp_init_copy(a, b) fp_copy(b, a)

/* clamp digits */
#define fp_clamp(a)   { while ((a)->used && (a)->dp[(a)->used-1] == 0) --((a)->used); if ((a)->sign != FP_ZPOS) printf("NG used=%d sign=%d @ %s:%d\n", (a)->used,(a)->sign,__FILE__,__LINE__); (a)->sign = (a)->used ? (a)->sign : FP_ZPOS; }
/* unsigned comparison */
int fp_cmp_mag(fp_int *a, fp_int *b);
void fp_mod(fp_int *a, fp_int *b, fp_int *c);

/* computes x/R == x (mod N) via Montgomery Reduction */
void fp_montgomery_reduce(fp_int *a, fp_int *m, fp_digit mp);

/* VARIOUS LOW LEVEL STUFFS */
void s_fp_add(fp_int *a, fp_int *b, fp_int *c);
void s_fp_sub(fp_int *a, fp_int *b, fp_int *c);
