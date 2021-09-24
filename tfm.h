#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

typedef uint32_t fp_digit;
typedef uint64_t fp_word;

#define FP_SIZE 140
typedef struct {
	fp_digit dp[FP_SIZE];
	int used;
} fp_int;

#define fp_zero(a)  (void)memset((a), 0, sizeof(fp_int))
#define fp_clamp(a) while ((a)->used && (a)->dp[(a)->used-1] == 0) --((a)->used)
