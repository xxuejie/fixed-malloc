/*
 * fixed-malloc in single header format
 */
#ifndef FIXED_MALLOC_ALL_H_
#define FIXED_MALLOC_ALL_H_

/* c-list.h */
/*
 * Original code is taken from:
 * https://github.com/c-util/c-list/blob/9aa81d84cadc67e92b441c89f84c57e72dd1e8a9/src/c-list.h
 * We adhere to the Apache-2.0 license here.
 */
#pragma once

/*
 * Circular Intrusive Double Linked List Collection in ISO-C11
 *
 * This implements a generic circular double linked list. List entries must
 * embed the CList object, which provides pointers to the next and previous
 * element. Insertion and removal can be done in O(1) due to the double links.
 * Furthermore, the list is circular, thus allows access to front/tail in O(1)
 * as well, even if you only have a single head pointer (which is not how the
 * list is usually operated, though).
 *
 * Note that you are free to use the list implementation without a head
 * pointer. However, usual operation uses a single CList object as head, which
 * is itself linked in the list and as such must be identified as list head.
 * This allows very simply list operations and avoids a lot of special cases.
 * Most importantly, you can unlink entries without requiring a head pointer.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct CList CList;

/**
 * struct CList - Entry of a circular double linked list
 * @next:               next entry
 * @prev:               previous entry
 *
 * Each entry in a list must embed a CList object. This object contains
 * pointers to its next and previous elements, which can be freely accessed by
 * the API user at any time. Note that the list is circular, and the list head
 * is linked in the list as well.
 *
 * The list head must be initialized via C_LIST_INIT before use. There is no
 * reason to initialize entry objects before linking them. However, if you need
 * a boolean state that tells you whether the entry is linked or not, you should
 * initialize the entry via C_LIST_INIT as well.
 */
struct CList {
        CList *next;
        CList *prev;
};

#define C_LIST_INIT(_var) { .next = &(_var), .prev = &(_var) }

/**
 * c_list_init() - initialize list entry
 * @what:               list entry to initialize
 *
 * Return: @what is returned.
 */
static inline CList *c_list_init(CList *what) {
        *what = (CList)C_LIST_INIT(*what);
        return what;
}

/**
 * c_list_entry_offset() - get parent container of list entry
 * @what:               list entry, or NULL
 * @offset:             offset of the list member in its surrounding type
 *
 * If the list entry @what is embedded into a surrounding structure, this will
 * turn the list entry pointer @what into a pointer to the parent container
 * (sometimes called container_of(3)). Use the `c_list_entry()` macro for an
 * easier API.
 *
 * If @what is NULL, this will also return NULL.
 *
 * Return: Pointer to parent container, or NULL.
 */
static inline void *c_list_entry_offset(const CList *what, size_t offset) {
        if (what) {
            /*
             * We allow calling "c_list_entry()" on the list head, which is
             * commonly a plain CList struct. The returned entry pointer is
             * thus invalid. For instance, this is used by the
             * c_list_for_each_entry*() macros. Gcc correctly warns about that
             * with "-Warray-bounds". However, as long as the value is never
             * dereferenced, this is fine. We explicitly use integer arithmetic
             * to circumvent the Gcc warning.
             */
            return (void *)(((uintptr_t)(void *)what) - offset);
        }
        return NULL;
}

/**
 * c_list_entry() - get parent container of list entry
 * @_what:              list entry, or NULL
 * @_t:                 type of parent container
 * @_m:                 member name of list entry in @_t
 *
 * If the list entry @_what is embedded into a surrounding structure, this will
 * turn the list entry pointer @_what into a pointer to the parent container
 * (using offsetof(3), or sometimes called container_of(3)).
 *
 * If @_what is NULL, this will also return NULL.
 *
 * Return: Pointer to parent container, or NULL.
 */
#define c_list_entry(_what, _t, _m) \
        ((_t *)c_list_entry_offset((_what), offsetof(_t, _m)))

/**
 * c_list_is_linked() - check whether an entry is linked
 * @what:               entry to check, or NULL
 *
 * Return: True if @what is linked in a list, false if not.
 */
static inline _Bool c_list_is_linked(const CList *what) {
        return what && what->next != what;
}

/**
 * c_list_is_empty() - check whether a list is empty
 * @list:               list to check, or NULL
 *
 * This is the same as !c_list_is_linked().
 *
 * Return: True if @list is empty, false if not.
 */
static inline _Bool c_list_is_empty(const CList *list) {
        return !c_list_is_linked(list);
}

/**
 * c_list_link_before() - link entry into list
 * @where:              linked list entry used as anchor
 * @what:               entry to link
 *
 * This links @what directly in front of @where. @where can either be a list
 * head or any entry in the list.
 *
 * If @where points to the list head, this effectively links @what as new tail
 * element. Hence, the macro c_list_link_tail() is an alias to this.
 *
 * @what is not inspected prior to being linked. Hence, it better not be linked
 * into another list, or the other list will be corrupted.
 */
static inline void c_list_link_before(CList *where, CList *what) {
        CList *prev = where->prev, *next = where;

        next->prev = what;
        what->next = next;
        what->prev = prev;
        prev->next = what;
}
#define c_list_link_tail(_list, _what) c_list_link_before((_list), (_what))

/**
 * c_list_link_after() - link entry into list
 * @where:              linked list entry used as anchor
 * @what:               entry to link
 *
 * This links @what directly after @where. @where can either be a list head or
 * any entry in the list.
 *
 * If @where points to the list head, this effectively links @what as new front
 * element. Hence, the macro c_list_link_front() is an alias to this.
 *
 * @what is not inspected prior to being linked. Hence, it better not be linked
 * into another list, or the other list will be corrupted.
 */
static inline void c_list_link_after(CList *where, CList *what) {
        CList *prev = where, *next = where->next;

        next->prev = what;
        what->next = next;
        what->prev = prev;
        prev->next = what;
}
#define c_list_link_front(_list, _what) c_list_link_after((_list), (_what))

/**
 * c_list_unlink_stale() - unlink element from list
 * @what:               element to unlink
 *
 * This unlinks @what. If @what was initialized via C_LIST_INIT(), it has no
 * effect. If @what was never linked, nor initialized, behavior is undefined.
 *
 * Note that this does not modify @what. It just modifies the previous and next
 * elements in the list to no longer reference @what. If you want to make sure
 * @what is re-initialized after removal, use c_list_unlink().
 */
static inline void c_list_unlink_stale(CList *what) {
        CList *prev = what->prev, *next = what->next;

        next->prev = prev;
        prev->next = next;
}

/**
 * c_list_unlink() - unlink element from list and re-initialize
 * @what:               element to unlink
 *
 * This is like c_list_unlink_stale() but re-initializes @what after removal.
 */
static inline void c_list_unlink(CList *what) {
        /* condition is not needed, but avoids STOREs in fast-path */
        if (c_list_is_linked(what)) {
                c_list_unlink_stale(what);
                *what = (CList)C_LIST_INIT(*what);
        }
}

/**
 * c_list_swap() - exchange the contents of two lists
 * @list1:      the list to operate on
 * @list2:      the list to operate on
 *
 * This replaces the contents of the list @list1 with the contents
 * of @list2, and vice versa.
 */
static inline void c_list_swap(CList *list1, CList *list2) {
        CList t;

        /* make neighbors of list1 point to list2, and vice versa */
        t = *list1;
        t.next->prev = list2;
        t.prev->next = list2;
        t = *list2;
        t.next->prev = list1;
        t.prev->next = list1;

        /* swap list1 and list2 now that their neighbors were fixed up */
        t = *list1;
        *list1 = *list2;
        *list2 = t;
}

/**
 * c_list_splice() - splice one list into another
 * @target:     the list to splice into
 * @source:     the list to splice
 *
 * This removes all the entries from @source and splice them into @target.
 * The order of the two lists is preserved and the source is appended
 * to the end of target.
 *
 * On return, the source list will be empty.
 */
static inline void c_list_splice(CList *target, CList *source) {
        if (!c_list_is_empty(source)) {
                /* attach the front of @source to the tail of @target */
                source->next->prev = target->prev;
                target->prev->next = source->next;

                /* attach the tail of @source to the front of @target */
                source->prev->next = target;
                target->prev = source->prev;

                /* clear source */
                *source = (CList)C_LIST_INIT(*source);
        }
}

/**
 * c_list_split() - split one list in two
 * @source:     the list to split
 * @where:      new starting element of newlist
 * @target:     new list
 *
 * This splits @source in two. All elements following @where (including @where)
 * are moved to @target, replacing any old list. If @where points to @source
 * (i.e., the end of the list), @target will be empty.
 */
static inline void c_list_split(CList *source, CList *where, CList *target) {
        if (where == source) {
                *target = (CList)C_LIST_INIT(*target);
        } else {
                target->next = where;
                target->prev = source->prev;

                where->prev->next = source;
                source->prev = where->prev;

                where->prev = target;
                target->prev->next = target;
        }
}

/**
 * c_list_first() - return pointer to first element, or NULL if empty
 * @list:               list to operate on, or NULL
 *
 * This returns a pointer to the first element, or NULL if empty. This never
 * returns a pointer to the list head.
 *
 * Return: Pointer to first list element, or NULL if empty.
 */
static inline CList *c_list_first(CList *list) {
        return c_list_is_empty(list) ? NULL : list->next;
}

/**
 * c_list_last() - return pointer to last element, or NULL if empty
 * @list:               list to operate on, or NULL
 *
 * This returns a pointer to the last element, or NULL if empty. This never
 * returns a pointer to the list head.
 *
 * Return: Pointer to last list element, or NULL if empty.
 */
static inline CList *c_list_last(CList *list) {
        return c_list_is_empty(list) ? NULL : list->prev;
}

/**
 * c_list_first_entry() - return pointer to first entry, or NULL if empty
 * @_list:              list to operate on, or NULL
 * @_t:                 type of list entries
 * @_m:                 name of CList member in @_t
 *
 * This is like c_list_first(), but also applies c_list_entry() on the result.
 *
 * Return: Pointer to first list entry, or NULL if empty.
 */
#define c_list_first_entry(_list, _t, _m) \
        c_list_entry(c_list_first(_list), _t, _m)

/**
 * c_list_last_entry() - return pointer to last entry, or NULL if empty
 * @_list:              list to operate on, or NULL
 * @_t:                 type of list entries
 * @_m:                 name of CList member in @_t
 *
 * This is like c_list_last(), but also applies c_list_entry() on the result.
 *
 * Return: Pointer to last list entry, or NULL if empty.
 */
#define c_list_last_entry(_list, _t, _m) \
        c_list_entry(c_list_last(_list), _t, _m)

/**
 * c_list_for_each*() - iterators
 *
 * The c_list_for_each*() macros provide simple for-loop wrappers to iterate
 * a linked list. They come in a set of flavours:
 *
 *   - "entry": This combines c_list_entry() with the loop iterator, so the
 *              iterator always has the type of the surrounding object, rather
 *              than CList.
 *
 *   - "safe": The loop iterator always keeps track of the next element to
 *             visit. This means, you can safely modify the current element,
 *             while retaining loop-integrity.
 *             You still must not touch any other entry of the list. Otherwise,
 *             the loop-iterator will be corrupted.
 *
 *   - "continue": Rather than starting the iteration at the front of the list,
 *                 use the current value of the iterator as starting position.
 *                 Note that the first loop iteration will be the following
 *                 element, not the given element.
 *
 *   - "unlink": This unlinks the current element from the list before the loop
 *               code is run. Note that this only does a partial unlink, since
 *               it assumes the entire list will be unlinked. You must not
 *               break out of the loop, or the list will be in an inconsistent
 *               state.
 */

/* direct/raw iterators */

#define c_list_for_each(_iter, _list)                                           \
        for (_iter = (_list)->next;                                             \
             (_iter) != (_list);                                                \
             _iter = (_iter)->next)

#define c_list_for_each_safe(_iter, _safe, _list)                               \
        for (_iter = (_list)->next, _safe = (_iter)->next;                      \
             (_iter) != (_list);                                                \
             _iter = (_safe), _safe = (_safe)->next)

#define c_list_for_each_continue(_iter, _list)                                  \
        for (_iter = (_iter) ? (_iter)->next : (_list)->next;                   \
             (_iter) != (_list);                                                \
             _iter = (_iter)->next)

#define c_list_for_each_safe_continue(_iter, _safe, _list)                      \
        for (_iter = (_iter) ? (_iter)->next : (_list)->next,                   \
             _safe = (_iter)->next;                                             \
             (_iter) != (_list);                                                \
             _iter = (_safe), _safe = (_safe)->next)

#define c_list_for_each_safe_unlink(_iter, _safe, _list)                        \
        for (_iter = (_list)->next, _safe = (_iter)->next;                      \
             c_list_init(_iter) != (_list);                                     \
             _iter = (_safe), _safe = (_safe)->next)

/* c_list_entry() based iterators */

#define c_list_for_each_entry(_iter, _list, _m)                                 \
        for (_iter = c_list_entry((_list)->next, __typeof__(*_iter), _m);       \
             &(_iter)->_m != (_list);                                           \
             _iter = c_list_entry((_iter)->_m.next, __typeof__(*_iter), _m))

#define c_list_for_each_entry_safe(_iter, _safe, _list, _m)                     \
        for (_iter = c_list_entry((_list)->next, __typeof__(*_iter), _m),       \
             _safe = c_list_entry((_iter)->_m.next, __typeof__(*_iter), _m);    \
             &(_iter)->_m != (_list);                                           \
             _iter = (_safe),                                                   \
             _safe = c_list_entry((_safe)->_m.next, __typeof__(*_iter), _m))

#define c_list_for_each_entry_continue(_iter, _list, _m)                        \
        for (_iter = c_list_entry((_iter) ? (_iter)->_m.next : (_list)->next,   \
                                  __typeof__(*_iter),                           \
                                  _m);                                          \
             &(_iter)->_m != (_list);                                           \
             _iter = c_list_entry((_iter)->_m.next, __typeof__(*_iter), _m))

#define c_list_for_each_entry_safe_continue(_iter, _safe, _list, _m)            \
        for (_iter = c_list_entry((_iter) ? (_iter)->_m.next : (_list)->next,   \
                                  __typeof__(*_iter),                           \
                                  _m),                                          \
             _safe = c_list_entry((_iter)->_m.next, __typeof__(*_iter), _m);    \
             &(_iter)->_m != (_list);                                           \
             _iter = (_safe),                                                   \
             _safe = c_list_entry((_safe)->_m.next, __typeof__(*_iter), _m))

#define c_list_for_each_entry_safe_unlink(_iter, _safe, _list, _m)              \
        for (_iter = c_list_entry((_list)->next, __typeof__(*_iter), _m),       \
             _safe = c_list_entry((_iter)->_m.next, __typeof__(*_iter), _m);    \
             c_list_init(&(_iter)->_m) != (_list);                              \
             _iter = (_safe),                                                   \
             _safe = c_list_entry((_safe)->_m.next, __typeof__(*_iter), _m))

/**
 * c_list_flush() - flush all entries from a list
 * @list:               list to flush
 *
 * This unlinks all entries from the given list @list and reinitializes their
 * link-nodes via C_LIST_INIT().
 *
 * Note that the entries are not modified in any other way, nor is their memory
 * released. This function just unlinks them and resets all the list nodes. It
 * is particularly useful with temporary lists on the stack in combination with
 * the GCC-extension __attribute__((__cleanup__(arg))).
 */
static inline void c_list_flush(CList *list) {
        CList *iter, *safe;

        c_list_for_each_safe_unlink(iter, safe, list)
                /* empty */ ;
}

/**
 * c_list_length() - return number of linked entries, excluding the head
 * @list:               list to operate on
 *
 * Returns the number of entries in the list, excluding the list head @list.
 * That is, for a list that is empty according to c_list_is_empty(), the
 * returned length is 0. This requires to iterate the list and has thus O(n)
 * runtime.
 *
 * Note that this function is meant for debugging purposes only. If you need
 * the list size during normal operation, you should maintain a counter
 * separately.
 *
 * Return: Number of items in @list.
 */
static inline size_t c_list_length(const CList *list) {
        size_t n = 0;
        const CList *iter;

        c_list_for_each(iter, list)
                ++n;

        return n;
}

/**
 * c_list_contains() - check whether an entry is linked in a certain list
 * @list:               list to operate on
 * @what:               entry to look for
 *
 * This checks whether @what is linked into @list. This requires a linear
 * search through the list, as such runs in O(n). Note that the list-head is
 * considered part of the list, and hence this returns true if @what equals
 * @list.
 *
 * Note that this function is meant for debugging purposes, and consistency
 * checks. You should always be aware whether your objects are linked in a
 * specific list.
 *
 * Return: True if @what is in @list, false otherwise.
 */
static inline _Bool c_list_contains(const CList *list, const CList *what) {
        const CList *iter;

        c_list_for_each(iter, list)
                if (what == iter)
                        return 1;

        return what == list;
}

#ifdef __cplusplus
}
#endif

/* utils.h */
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

/* linear-malloc.h */
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

/* slab-malloc.h */
#ifndef FIXED_MALLOC_SLAB_MALLOC_H_
#define FIXED_MALLOC_SLAB_MALLOC_H_

#include <stddef.h>

int fm_sm_reinit(void *buffer, size_t size, int zero_filled);
void *fm_sm_malloc(size_t size);
void fm_sm_free(void *ptr);
void *fm_sm_realloc(void *ptr, size_t size);

#endif /* FIXED_MALLOC_SLAB_MALLOC_H_ */

#ifndef FIXED_MALLOC_DECLARATION_ONLY

/* linear-malloc.c */
/* #include "linear-malloc.h" */

#include <string.h>

/* #include "c-list.h" */
/* #include "utils.h" */

typedef struct region_t {
  CList link;
  uint64_t start_page;
  uint64_t pages;
} region_t;

typedef struct meta_t {
  // Linear malloc supports a maximum of 4GB memory size, which is 1024
  // pages at most.
  uint8_t pages[1024];
} meta_t;

#ifndef FM_MANUAL_INIT
#ifndef FM_MEMORY_SIZE
#define FM_MEMORY_SIZE (640 * 1024)
#endif
#if (FM_MEMORY_SIZE & (FM_PAGE_SIZE - 1)) != 0
#error "Linear malloc memory size must be aligned on 4KB!"
#endif
#if (FM_MEMORY_SIZE < 128 * 1024) || (FM_MEMORY_SIZE > 0xFFFFFFFF)
#error "Linear malloc memory size must be between 128KB and 4GB!"
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
  if ((size < 128 * 1024) || (size > 0xFFFFFFFF)) {
    FM_DEBUG("Memory size must be between 128KB and 4GB!");
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

/* slab-malloc.c */
/* #include "slab-malloc.h" */

#include <string.h>

/* #include "c-list.h" */
/* #include "linear-malloc.h" */
/* #include "utils.h" */

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

#endif /* FIXED_MALLOC_DECLARATION_ONLY */

#endif /* FIXED_MALLOC_ALL_H_ */
