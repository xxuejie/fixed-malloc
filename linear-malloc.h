#ifndef FIXED_MALLOC_LINEAR_MALLOC_H_
#define FIXED_MALLOC_LINEAR_MALLOC_H_

#include <stddef.h>

#define FM_PAGE_SHIFT 12
// 4096
#define FM_PAGE_SIZE (1 << FM_PAGE_SHIFT)

#define FM_LM_T_TRANSIENT 0x1
#define FM_LM_T_PERSISTENT 0x2

int fm_lm_reinit(void *buffer, size_t size, int zero_filled);
void *fm_lm_malloc(size_t size, int t);
void fm_lm_free(void *ptr);
void *fm_lm_realloc(void *ptr, size_t size, int t);

#endif /* FIXED_MALLOC_LINEAR_MALLOC_H_ */
