#ifndef FIXED_MALLOC_SLAB_MALLOC_H_
#define FIXED_MALLOC_SLAB_MALLOC_H_

#include <stddef.h>

int fm_sm_reinit(void *buffer, size_t size, int zero_filled);
void *fm_sm_malloc(size_t size);
void fm_sm_free(void *ptr);
void *fm_sm_realloc(void *ptr, size_t size);

#endif /* FIXED_MALLOC_SLAB_MALLOC_H_ */
