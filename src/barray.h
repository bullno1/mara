#ifndef BARRAY_H
#define BARRAY_H

#include <stddef.h>

#ifndef BARRAY_REALLOC
#include <stdlib.h>
#define BARRAY_REALLOC(ctx, ptr, size) barray_realloc(ctx, ptr, size)
#define BARRAY_USE_STD_REALLOC
#endif

#ifndef BARRAY_CTX_TYPE
#define BARRAY_CTX_TYPE void*
#endif

#define barray(T) T*

#define barray_push(ctx, array, element) \
	do { \
		size_t barray__new_len; \
		array = barray_prepare_push(ctx, array, &barray__new_len, sizeof(element)); \
		array[barray__new_len - 1] = element; \
	} while (0)

#define barray_reserve(ctx, array, new_capacity) \
	do { \
		array = barray_do_reserve(ctx, array, new_capacity, sizeof(*(array)); \
	} while (0)

#define barray_resize(ctx, array, new_len) \
	do { \
		array = barray_do_resize(ctx, array, new_len, sizeof(*(array))); \
	} while (0)

#define barray_pop(array) \
	array[barray_len(array) - 1], barray_do_pop(array)

typedef struct {
	size_t capacity;
	size_t len;
	_Alignas(max_align_t) char elems[];
} barray_header_t;

#ifdef BARRAY_USE_STD_REALLOC

static inline void*
barray_realloc(void* ctx, void* ptr, size_t size) {
	(void)ctx;
	if (size > 0) {
		return realloc(ptr, size);
	} else {
		free(ptr);
		return NULL;
	}
}

#endif

static inline barray_header_t*
barray_header_of(void* array) {
	if (array != NULL) {
		return (barray_header_t*)((char*)array - offsetof(barray_header_t, elems));
	} else {
		return NULL;
	}
}

static inline size_t
barray_len(void* array) {
	barray_header_t* header = barray_header_of(array);
	return header != NULL ? header->len : 0;
}

static inline size_t
barray_capacity(void* array) {
	barray_header_t* header = barray_header_of(array);
	return header != NULL ? header->capacity : 0;
}

static inline void
barray_free(BARRAY_CTX_TYPE ctx, void* array) {
	barray_header_t* header = barray_header_of(array);
	if (header != NULL) {
		BARRAY_REALLOC(ctx, header, 0);
	}
}

static inline void*
barray_prepare_push(BARRAY_CTX_TYPE ctx, void* array, size_t* new_len, size_t elem_size) {
	barray_header_t* header = barray_header_of(array);
	size_t len = header != NULL ? header->len : 0;
	size_t capacity = header != NULL ? header->capacity : 0;

	if (len < capacity) {
		header->len = *new_len = len + 1;
		return array;
	} else {
		size_t new_capacity = capacity > 0 ? capacity * 2 : 2;
		barray_header_t* new_header = BARRAY_REALLOC(
			ctx, header, sizeof(barray_header_t) + elem_size * new_capacity
		);
		new_header->capacity = new_capacity;
		new_header->len = *new_len = len + 1;
		return new_header->elems;
	}
}

static inline void*
barray_do_reserve(BARRAY_CTX_TYPE ctx, void* array, size_t new_capacity, size_t elem_size) {
	barray_header_t* header = barray_header_of(array);
	size_t current_capacity = header != NULL ? header->capacity : 0;
	if (new_capacity <= current_capacity) {
		return array;
	}

	barray_header_t* new_header = BARRAY_REALLOC(
		ctx, header, sizeof(barray_header_t) + elem_size * new_capacity
	);
	new_header->capacity = new_capacity;
	return new_header->elems;
}

static inline void*
barray_do_resize(BARRAY_CTX_TYPE ctx, void* array, size_t new_len, size_t elem_size) {
	barray_header_t* header = barray_header_of(array);
	size_t current_capacity = header != NULL ? header->capacity : 0;

	if (new_len <= current_capacity) {
		header->len = new_len;
		return array;
	} else {
		barray_header_t* new_header = BARRAY_REALLOC(
			ctx, header, sizeof(barray_header_t) + elem_size * new_len
		);
		new_header->capacity = new_len;
		new_header->len = new_len;
		return new_header->elems;
	}
}

static inline void
barray_do_pop(void* array) {
	barray_header_t* header = barray_header_of(array);
	header->len -= 1;
}

#endif
