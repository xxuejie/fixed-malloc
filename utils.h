#ifndef FIXED_MALLOC_UTILS_H_
#define FIXED_MALLOC_UTILS_H_

#include <stddef.h>

static inline size_t __fm_roundup(size_t x, size_t round) {
  return x + (((~x) + 1) & (round - 1));
}

static inline size_t __fm_rounddown(size_t x, size_t round) {
  return x & (~(round - 1));
}

#ifndef FM_DEBUG
#include <stdio.h>
#define FM_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef FM_PRINT
#include <stdio.h>
#define FM_PRINT(...) printf(__VA_ARGS__)
#endif

#ifndef FM_ABORT
#include <stdlib.h>
#define FM_ABORT abort
#endif

#endif /* FIXED_MALLOC_UTILS_H_ */
