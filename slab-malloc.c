#include "slab-malloc.h"

#include <string.h>

#include "c-list.h"
#include "linear-malloc.h"
#include "utils.h"

#define FM_SM_INVALID_SLAB 0xFFFFFFFF

static size_t slab_sizes[] = {32, 64, 128, 512, 1024};
static CList slab_lists[] = {
    C_LIST_INIT(slab_lists[0]), C_LIST_INIT(slab_lists[1]),
    C_LIST_INIT(slab_lists[2]), C_LIST_INIT(slab_lists[3]),
    C_LIST_INIT(slab_lists[4]),
};

int fm_sm_reinit(void *buffer, size_t size, int zero_filled) {
  int ret = fm_lm_reinit(buffer, size, zero_filled);
  if (ret != 0) {
    return ret;
  }
  for (size_t i = 0; i < sizeof(slab_lists) / sizeof(CList); i++) {
    c_list_init(&slab_lists[i]);
  }
  return 0;
}

static size_t slab_index(size_t size) {
  // Right now we only have 5 slabs, a linear search shall be enough,
  // a binary search might be needed once we have more slabs.
  for (size_t i = 0; i < sizeof(slab_sizes) / sizeof(size_t); i++) {
    if (size <= slab_sizes[i]) {
      return i;
    }
  }
  return FM_SM_INVALID_SLAB;
}

typedef struct page_meta_t {
  CList link;
  uint64_t bitmap[2];
  size_t size;
  size_t count;
  size_t slab_index;
  size_t _padding;
} page_meta_t;

#define PAGE_META_RESERVED_SIZE 64

static size_t ptr_to_index(const page_meta_t *meta, const void *ptr) {
  size_t p = (size_t)ptr;
  size_t base = ((size_t)meta) + PAGE_META_RESERVED_SIZE;
#ifdef FM_GUARDS
  if ((p - base) % meta->size != 0) {
    FM_DEBUG("Pointer does not lie on the boundary of slab allocated value!");
    FM_ABORT();
  }
  if ((p - base) / meta->size >= meta->count) {
    FM_DEBUG("Pointer exceeds slab count!");
    FM_ABORT();
  }
#endif
  return (p - base) / meta->size;
}

static void *index_to_ptr(const page_meta_t *meta, size_t index) {
#ifdef FM_GUARDS
  if (index >= meta->count) {
    FM_DEBUG("Invalid index in slab!");
    FM_ABORT();
  }
#endif
  return (void *)(((size_t)meta) + PAGE_META_RESERVED_SIZE +
                  index * meta->size);
}

static int bitmap_all_cleared(const page_meta_t *meta) {
  return (meta->bitmap[0] == 0) && (meta->bitmap[1] == 0);
}

static int bitmap_all_used(const page_meta_t *meta) {
  return (size_t)(__builtin_popcountl(meta->bitmap[0]) +
                  __builtin_popcountl(meta->bitmap[1])) == meta->count;
}

static size_t bitmap_next_free(const page_meta_t *meta) {
  size_t zeros = FM_SM_INVALID_SLAB;
  if (meta->bitmap[0] != (uint64_t)-1) {
    zeros = __builtin_ctzl(~(meta->bitmap[0]));
  } else if (meta->bitmap[1] != (uint64_t)-1) {
    zeros = 64 + __builtin_ctzl(~(meta->bitmap[1]));
  }
  if (zeros >= meta->count) {
    return FM_SM_INVALID_SLAB;
  }
  return zeros;
}

static void bitmap_set(page_meta_t *meta, size_t index) {
  meta->bitmap[index / 64] |= ((uint64_t)1) << (index % 64);
}

static void bitmap_clear(page_meta_t *meta, size_t index) {
  meta->bitmap[index / 64] &= (~(((uint64_t)1) << (index % 64)));
}

void fm_sm_free(void *ptr) {
  if ((((size_t)ptr) & (FM_PAGE_SIZE - 1)) == 0) {
    fm_lm_free(ptr);
    return;
  }
  page_meta_t *meta = (page_meta_t *)__fm_rounddown((size_t)ptr, FM_PAGE_SIZE);
  size_t element_index = ptr_to_index(meta, ptr);
  int all_used = bitmap_all_used(meta);
  bitmap_clear(meta, element_index);
  if (all_used) {
    c_list_link_tail(&slab_lists[meta->slab_index], &meta->link);
    FM_DEBUG("Retrieving previously fully used slab: %p %ld\n", meta,
             meta->size);
  }
}

void *fm_sm_realloc(void *ptr, size_t size) {
  if ((((size_t)ptr) & (FM_PAGE_SIZE - 1)) == 0) {
    return fm_lm_realloc(ptr, size, FM_LM_T_TRANSIENT);
  }
  page_meta_t *meta = (page_meta_t *)__fm_rounddown((size_t)ptr, FM_PAGE_SIZE);
  if (size <= meta->size) {
    return ptr;
  }
  void *p = fm_sm_malloc(size);
  if (p != NULL) {
    memcpy(p, ptr, meta->size);
    fm_sm_free(ptr);
  }
  return p;
}

static void free_empty_slabs() {
  for (size_t i = 0; i < sizeof(slab_sizes) / sizeof(size_t); i++) {
    CList *iter = slab_lists[i].next;
    while (iter != &slab_lists[i]) {
      page_meta_t *meta = c_list_entry(iter, page_meta_t, link);
      CList *old = iter;
      iter = iter->next;
      if (bitmap_all_cleared(meta)) {
        c_list_unlink(old);
        fm_lm_free(meta);
      }
    }
  }
}

static void *lm_malloc(size_t size, int t) {
  void *p = fm_lm_malloc(size, t);
  if (p == NULL) {
    // When previous attempt fails, try freeing empty slabs, then retry
    free_empty_slabs();
    p = fm_lm_malloc(size, t);
  }
  return p;
}

void *fm_sm_malloc(size_t size) {
  size_t i = slab_index(size);
  if (i == FM_SM_INVALID_SLAB) {
    return lm_malloc(size, FM_LM_T_TRANSIENT);
  }
  for (CList *iter = slab_lists[i].next; iter != &slab_lists[i];
       iter = iter->next) {
    page_meta_t *meta = c_list_entry(iter, page_meta_t, link);
    size_t index = bitmap_next_free(meta);
    if (index != FM_SM_INVALID_SLAB) {
      bitmap_set(meta, index);
      if (bitmap_all_used(meta)) {
        c_list_unlink(iter);
        FM_DEBUG("Unlinking fully utilized slab: %p %ld\n", meta, meta->size);
      }
      return index_to_ptr(meta, index);
    }
  }
  // Create a new slab here
  void *slab = fm_lm_malloc(FM_PAGE_SIZE, FM_LM_T_PERSISTENT);
  if (slab == NULL) {
    return NULL;
  }
  page_meta_t *meta = (page_meta_t *)slab;
  meta->bitmap[0] = 0;
  meta->bitmap[1] = 0;
  meta->size = slab_sizes[i];
  meta->slab_index = i;
  meta->count = (FM_PAGE_SIZE - PAGE_META_RESERVED_SIZE) / meta->size;
  c_list_link_front(&slab_lists[i], &meta->link);
  FM_DEBUG("Creating new slab: %p %ld\n", meta, meta->size);

  size_t element_index = 0;
  bitmap_set(meta, element_index);
  return index_to_ptr(meta, element_index);
}
