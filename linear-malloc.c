#include "linear-malloc.h"

#include <string.h>

#include "c-list.h"
#include "utils.h"

typedef struct region_t {
  CList link;
  uint64_t start_page;
  uint64_t pages;
} region_t;

typedef struct meta_t {
  // Linear malloc supports a maximum of 4096 pages now, which is 16MB.
  uint8_t pages[4096];
} meta_t;

#ifndef FM_MANUAL_INIT
#ifndef FM_MEMORY_SIZE
#define FM_MEMORY_SIZE (640 * 1024)
#endif
#if (FM_MEMORY_SIZE & (FM_PAGE_SIZE - 1)) != 0
#error "Linear malloc memory size must be aligned on 4KB!"
#endif
#if (FM_MEMORY_SIZE < 128 * 1024) || (FM_MEMORY_SIZE >= 16 * 1024 * 1024)
#error "Linear malloc memory size must be between 128KB and 16MB!"
#endif

static uint8_t __sbuffer[FM_MEMORY_SIZE]
    __attribute__((aligned(FM_PAGE_SIZE))) = {0};
// Forward declaration
static CList __free_regions;
// Here we employ a slight hack so we can initialize everything at compile time.
static region_t __initial_region = {
    .link = {&__free_regions, &__free_regions},
    // The first page is set aside for accounting purposes
    .start_page = 1,
    .pages = FM_MEMORY_SIZE / FM_PAGE_SIZE - 1,
};
static uint8_t *__buffer_start = __sbuffer;
static CList __free_regions = {
    .next = &__initial_region.link,
    .prev = &__initial_region.link,
};
static meta_t *__meta = (meta_t *)__sbuffer;
#else
static uint8_t *__buffer_start = NULL;
static CList __free_regions = C_LIST_INIT(__free_regions);
static meta_t *__meta = NULL;
#endif

static CList __freed_memories = C_LIST_INIT(__freed_memories);

#ifdef FM_TEST_SUPPORT
#include <stdio.h>

void fm_lm_test_dump_regions(CList *first, const char *name, int max) {
  (void)name;
  FM_PRINT("### Region %s list first: %p\n", name, first);
  int i = 0;
  CList *iter = first->next;
  while (i < max && iter != first) {
    region_t *region = c_list_entry(iter, region_t, link);
    (void)region;
    FM_PRINT(
        "  Entry %d iter pointer: %p, actual pointer %p, start: %ld, pages: "
        "%ld\n",
        i, iter, region, region->start_page, region->pages);
    iter = iter->next;
    i++;
  }
  if (iter != first) {
    FM_PRINT(
        "WARNING: there are more entries available, either max is too short, "
        "or there is an infinite loop!\n");
  }
  FM_PRINT("### Region %s ends.\n", name);
}

#ifndef FM_MANUAL_INIT
static size_t __buffer_size = FM_MEMORY_SIZE;
#else
static size_t __buffer_size = 0;
#endif

void *fm_lm_test_buffer_pointer() { return __buffer_start; }

size_t fm_lm_test_total_buffer_size() { return __buffer_size; }
#endif

int fm_lm_reinit(void *buffer, size_t size, int zero_filled) {
  if ((((size_t)buffer) & (FM_PAGE_SIZE - 1)) != 0) {
    FM_DEBUG("Memory buffer must be aligned at 4K boundary!");
    FM_ABORT();
  }
  if ((size & (FM_PAGE_SIZE - 1)) != 0) {
    FM_DEBUG("Memory size must be aligned to 4K!");
    FM_ABORT();
  }
  if ((size < 128 * 1024) || (size >= 16 * 1024 * 1024)) {
    FM_DEBUG("Memory size must be between 128KB and 16MB!");
    FM_ABORT();
  }

  __buffer_start = buffer;
#ifdef FM_TEST_SUPPORT
  __buffer_size = size;
#endif
  __meta = buffer;
  if (!zero_filled) {
    memset(buffer, 0, FM_PAGE_SIZE);
  }
  region_t *region = (region_t *)(((uint8_t *)buffer) + FM_PAGE_SIZE);
  region->start_page = 1;
  region->pages = size / FM_PAGE_SIZE - 1;
  c_list_init(&__free_regions);
  c_list_link_after(&__free_regions, &region->link);
  c_list_init(&__freed_memories);
  return 0;
}

static void mark_alloced_pages(size_t first_page, size_t pages) {
  if (pages < 0xFF) {
    __meta->pages[first_page] = (uint8_t)pages;
  } else {
    __meta->pages[first_page] = 0xFF;
    size_t aligned_page = __fm_roundup(first_page + 1, 4);
    *((uint32_t *)(&__meta->pages[aligned_page])) = (uint32_t)pages;
  }
}

static size_t fetch_alloced_pages(size_t first_page) {
  uint8_t pages = __meta->pages[first_page];
  if (__builtin_expect(pages < 0xFF, 1)) {
    return pages;
  }
  size_t aligned_page = __fm_roundup(first_page + 1, 4);
  return *((uint32_t *)(&__meta->pages[aligned_page]));
}

static inline size_t ptr_to_page(void *ptr) {
  return (((size_t)ptr) - ((size_t)__buffer_start)) / FM_PAGE_SIZE;
}

static inline void *page_to_ptr(size_t page) {
  return (void *)(__buffer_start + (page * FM_PAGE_SIZE));
}

static inline region_t *move_region(const region_t *src) {
  region_t *dst = (region_t *)page_to_ptr(src->start_page);
  if (dst == src) {
    return dst;
  }

  memcpy(dst, src, sizeof(region_t));
  dst->link.next->prev = &dst->link;
  dst->link.prev->next = &dst->link;

  return dst;
}

void fm_lm_free(void *ptr) {
#ifdef FM_GUARDS
  if ((((size_t)ptr) & (FM_PAGE_SIZE - 1)) != 0) {
    FM_DEBUG(
        "Pointer passed to free is not aligned, which might be tampered with!");
    FM_ABORT();
  }
#endif
  size_t first_page = ptr_to_page(ptr);
  size_t pages = fetch_alloced_pages(first_page);
  region_t *region = (region_t *)ptr;
  region->start_page = first_page;
  region->pages = pages;
  c_list_link_tail(&__freed_memories, &region->link);
}

static size_t alloc_designated_free_pages(size_t start_page,
                                          size_t requested_pages) {
  for (CList *iter = __free_regions.next; iter != &__free_regions;
       iter = iter->next) {
    region_t *region = c_list_entry(iter, region_t, link);
    if (region->start_page == start_page && region->pages >= requested_pages) {
      size_t result = region->start_page;
      region->start_page += requested_pages;
      region->pages -= requested_pages;
      if (region->pages == 0) {
        c_list_unlink(iter);
      } else {
        move_region(region);
      }
      return result;
    }
  }
  return 0;
}

static size_t alloc_free_pages(size_t requested_pages) {
  for (CList *iter = __free_regions.next; iter != &__free_regions;
       iter = iter->next) {
    region_t *region = c_list_entry(iter, region_t, link);
    if (region->pages >= requested_pages) {
      size_t result = region->start_page;
      region->start_page += requested_pages;
      region->pages -= requested_pages;
      if (region->pages == 0) {
        c_list_unlink(iter);
      } else {
        // we need to move region to a new location, since the old location
        // has been allocated
        move_region(region);
      }
      return result;
    }
  }
  return 0;
}

void *fm_lm_realloc(void *ptr, size_t size, int t) {
  if (ptr == NULL) {
    return fm_lm_malloc(size, t);
  }
#ifdef FM_GUARDS
  if ((((size_t)ptr) & (FM_PAGE_SIZE - 1)) != 0) {
    FM_DEBUG(
        "Pointer passed to free is not aligned, which might be tampered with!");
    FM_ABORT();
  }
#endif
  size = __fm_roundup(size, FM_PAGE_SIZE);
  size_t new_pages = size / FM_PAGE_SIZE;
  size_t first_page = ptr_to_page(ptr);
  size_t pages = fetch_alloced_pages(first_page);
  if (new_pages <= pages) {
    return ptr;
  }
  size_t succeeding_pages =
      alloc_designated_free_pages(first_page + pages, new_pages - pages);
  if (succeeding_pages != 0) {
    // If there are enough free pages that are immediately after current
    // allocated memory, we won't need to move the pages elsewhere
    return ptr;
  }
  void *p = fm_lm_malloc(size, t);
  if (p != NULL) {
    memcpy(p, ptr, pages * FM_PAGE_SIZE);
    fm_lm_free(ptr);
  }
  return p;
}

static size_t alloc_free_pages_reverse(size_t requested_pages) {
  for (CList *iter = __free_regions.prev; iter != &__free_regions;
       iter = iter->prev) {
    region_t *region = c_list_entry(iter, region_t, link);
    if (region->pages >= requested_pages) {
      // The first page is untouched, there is no need to move region struct.
      size_t result = region->start_page + region->pages - requested_pages;
      region->pages -= requested_pages;
      if (region->pages == 0) {
        c_list_unlink(iter);
      }
      return result;
    }
  }
  return 0;
}

static void merged_consecutive_pages() {
  CList *prev_item = __free_regions.next;
  CList *current_item = prev_item->next;
  while (prev_item != &__free_regions && current_item != &__free_regions) {
    region_t *prev_region = c_list_entry(prev_item, region_t, link);
    region_t *current_region = c_list_entry(current_item, region_t, link);

    if (prev_region->start_page + prev_region->pages ==
        current_region->start_page) {
      prev_region->pages += current_region->pages;
      c_list_unlink(current_item);
      current_item = prev_item->next;
    } else {
      prev_item = current_item;
      current_item = current_item->next;
    }
  }
}

static void restore_freed_region(region_t *free_region) {
  CList *prev_item = &__free_regions;
  for (CList *iter = __free_regions.next; iter != &__free_regions;
       iter = iter->next) {
    region_t *region = c_list_entry(iter, region_t, link);
    if (free_region->start_page < region->start_page) {
      // Insert pages between prev_item and iter
      int inserted = 0;
      if (prev_item != &__free_regions) {
        region_t *prev_region = c_list_entry(prev_item, region_t, link);
        if (prev_region->start_page + prev_region->pages ==
            free_region->start_page) {
          // Attach free pages to the previous item
          prev_region->pages += free_region->pages;
          inserted = 1;
        }
      }
      if ((!inserted) && ((free_region->start_page + free_region->pages) ==
                          region->start_page)) {
        region->start_page = free_region->start_page;
        region->pages += free_region->pages;
        // Attach free pages to current item, we need to move region here
        region_t *new_region = move_region(region);
        iter = &new_region->link;
        inserted = 1;
      }
      if (inserted) {
        merged_consecutive_pages();
      } else {
        // Attach free pages as a new region
        c_list_link_before(iter, &free_region->link);
      }
      return;
    }
    prev_item = iter;
  }
  // Insert pages at the very end of the page. Notice at this stage, prev_item
  // contains the last item(if available)
  c_list_link_tail(&__free_regions, &free_region->link);
  merged_consecutive_pages();
}

static void restore_all_freed_memories() {
  CList *iter = __freed_memories.next;
  while (iter != &__freed_memories) {
    region_t *region = c_list_entry(iter, region_t, link);
    iter = iter->next;
    restore_freed_region(region);
  }
  c_list_init(&__freed_memories);
}

static inline size_t alloc(size_t pages, int t) {
  if (t == FM_LM_T_TRANSIENT) {
    return alloc_free_pages(pages);
  } else {
    return alloc_free_pages_reverse(pages);
  }
}

void *fm_lm_malloc(size_t size, int t) {
  size = __fm_roundup(size, FM_PAGE_SIZE);
  size_t pages = size / FM_PAGE_SIZE;

  size_t page = alloc(pages, t);
  if (page == 0) {
    restore_all_freed_memories();
    page = alloc(pages, t);
  }
  if (page == 0) {
    return NULL;
  }
  mark_alloced_pages(page, pages);
  return page_to_ptr(page);
}
