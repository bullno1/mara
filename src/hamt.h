#ifndef HAMT_H
#define HAMT_H

#include <stddef.h>

#ifndef HAMT_HASH_TYPE
#include <stdint.h>
#define HAMT_HASH_TYPE uint32_t
#endif

#ifndef HAMT_NUM_BITS
#define HAMT_NUM_BITS 3
#endif

#define HAMT_WIDTH (1 << HAMT_NUM_BITS)
#define HAMT_MASK (((HAMT_HASH_TYPE)1 << HAMT_NUM_BITS) - 1)

#ifndef HAMT_MALLOC
#include <stdlib.h>
#define HAMT_MALLOC(CTX, SIZE) malloc(SIZE)
#define HAMT_FREE(CTX, SIZE) free(SIZE)
#define HAMT_DEFAULT_MEMCTX (&malloc)
#endif

#ifndef HAMT_MEMSET
#include <string.h>
#define HAMT_MEMSET(PTR, VALUE, SIZE) memset(PTR, VALUE, SIZE)
#endif

#ifndef HAMT_ASSERT
#include <assert.h>
#define HAMT_ASSERT(COND, MSG) assert((COND) && MSG)
#endif

#ifndef HAMT_IS_TOMBSTONE
#define HAMT_TOMBSTONE char deleted;
#define HAMT_IS_TOMBSTONE(NODE) (NODE)->deleted
#define HAMT_SET_TOMBSTONE(NODE) (NODE)->deleted = 1
#define HAMT_UNSET_TOMBSTONE(NODE) (NODE)->deleted = 0
#else
#define HAMT_SEPARATE_TOMBSTONE
#endif

#ifndef HAMT_NO_ITERATION
#define HAMT_NEXT_PTR void* next;
#define HAMT_LINK(NODE, HEAD) \
	do { \
		if (HEAD != NULL) { \
			(NODE)->next = *(void**)HEAD; \
			*(void**)HEAD = (NODE); \
		} else { \
			(NODE)->next = NULL; \
		} \
	} while (0)
#else
#define HAMT_NEXT_PTR
#define HAMT_LINK(NODE, HEAD)
#endif

#define HAMT_MAP(KEY_TYPE, VALUE_TYPE) \
	struct { \
		HAMT_NEXT_PTR \
		void* children[HAMT_WIDTH]; \
		KEY_TYPE key; \
		VALUE_TYPE value; \
		HAMT_TOMBSTONE \
	}

#define HAMT_SET(KEY_TYPE) \
	struct { \
		HAMT_NEXT_PTR \
		void* children[HAMT_WIDTH]; \
		KEY_TYPE key; \
		HAMT_TOMBSTONE \
	}

#define hamt_get_node(MEMCTX, ITR, KEYEQ, KEYHASH, KEY) \
	do { \
		void* hamt_free_ = NULL; \
		void* hamt_head_ = *ITR; \
		(void)hamt_head_; \
		for (HAMT_HASH_TYPE hamt_hash_ = KEYHASH; *ITR != NULL; hamt_hash_ >>= HAMT_NUM_BITS) { \
			if (HAMT_IS_TOMBSTONE(*ITR)) { \
				hamt_free_ = *ITR; \
			} else if (KEYEQ((KEY), ((*ITR)->key))) { \
				break; \
			} \
			*(void**)&ITR = &(*ITR)->children[hamt_hash_ & HAMT_MASK]; \
		} \
		if (*ITR != NULL || MEMCTX == NULL) { break; } \
		if (hamt_free_ != NULL) { \
			*ITR = hamt_free_; \
			HAMT_UNSET_TOMBSTONE(*ITR); \
			break; \
		} \
		*ITR = HAMT_MALLOC(MEMCTX, sizeof(**ITR)); \
		HAMT_MEMSET(*ITR, 0, sizeof(**ITR)); \
		HAMT_UNSET_TOMBSTONE(*ITR); \
		HAMT_LINK(*ITR, hamt_head_); \
		(*ITR)->key = KEY; \
	} while (0)

#define hamt_upsert(MEMCTX, ITR, KEYEQ, KEYHASH, KEY, VALUE) \
	do { \
		HAMT_ASSERT(MEMCTX != NULL, "Must provide a memctx"); \
		hamt_get_node(MEMCTX, ITR, KEYEQ, KEYHASH, KEY); \
		(*ITR)->value = VALUE; \
	} while (0)

#define hamt_set_add(MEMCTX, ITR, KEYEQ, KEYHASH, KEY) \
	hamt_get_node(MEMCTX, ITR, KEYEQ, KEYHASH, KEY);

#define hamt_delete(ITR, KEYEQ, KEYHASH, KEY) \
	do { \
		hamt_get_node(NULL, ITR, KEYEQ, KEYHASH, KEY); \
		if (*ITR != NULL) { HAMT_SET_TOMBSTONE(*ITR); } \
	} while (0)

#define hamt_find(ITR, KEYEQ, KEYHASH, KEY) \
	hamt_get_node(NULL, ITR, KEYEQ, KEYHASH, KEY)

#ifndef HAMT_NO_ITERATION

#define hamt_foreach(ITR) \
	for (; (*ITR) != NULL; *(void**)&ITR = &(*ITR)->next) \
		if (!HAMT_IS_TOMBSTONE(*ITR))

#define hamt_free(MEMCTX, MAP) \
	do { \
		HAMT_ASSERT(MEMCTX != NULL, "Must provide a memctx"); \
		void* ptr = MAP; \
		while (ptr != NULL) { \
			void* next = *(void**)ptr; \
			HAMT_FREE(MEMCTX, ptr); \
			ptr = next; \
		} \
	} while (0)

#define hamt_clear(ITR) \
	do { \
		for (; (*ITR) != NULL; *(void**)&ITR = &(*ITR)->next) { \
			HAMT_SET_TOMBSTONE(*ITR); \
		} \
	} while (0)

#endif

#endif

#ifdef HAMT_EXAMPLE

#include <stdio.h>

#define KEYEQ(lhs, rhs) ((lhs) == (rhs))

int main(void) {
	HAMT_MAP(int, int)* a = NULL, **b, **itr;
	b = &a;
	hamt_upsert(HAMT_DEFAULT_MEMCTX, b, KEYEQ, 3, 3, 4);
	b = &a;
	hamt_upsert(HAMT_DEFAULT_MEMCTX, b, KEYEQ, 4, 4, 5);
	b = &a;
	hamt_upsert(HAMT_DEFAULT_MEMCTX, b, KEYEQ, 8, 8, 9);
	b = &a;
	hamt_foreach(b) {
		printf("%d => %d\n", (*b)->key, (*b)->value);
	}
	b = &a;
	printf("---------\n");
	hamt_delete(b, KEYEQ, 3, 3);
	b = &a;
	hamt_foreach(b) {
		printf("%d => %d\n", (*b)->key, (*b)->value);
	}
	printf("---------\n");
	b = &a;
	hamt_upsert(HAMT_DEFAULT_MEMCTX, b, KEYEQ, 8, 8, 8);
	b = &a;
	hamt_upsert(HAMT_DEFAULT_MEMCTX, b, KEYEQ, 3, 3, 9);
	b = &a;
	hamt_foreach(b) {
		itr = &a;
		hamt_find(itr, KEYEQ, (*b)->key, (*b)->key);
		printf("%d => %d\n", (*b)->key, (*itr)->value);
	}
	hamt_free(HAMT_DEFAULT_MEMCTX, a);
	b = NULL;
	a = NULL;
}

#endif
