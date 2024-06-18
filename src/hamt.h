#ifndef BHAMT_H
#define BHAMT_H

#ifndef BHAMT_HASH_TYPE
#include <stdint.h>
#define BHAMT_HASH_TYPE uint32_t
#endif

#ifndef BHAMT_NUM_BITS
#define BHAMT_NUM_BITS 2
#endif

#ifndef BHAMT_NODE_TYPE
// This is present in both MSVC and Clang.
// It should also be standard for C23.
// To avoid using extension, define it to a concrete type.
// For example: `#define BHAMT_NODE_TYPE(ROOT) my_node_type`.
#	if __STDC_VERSION__ >= 202311L
#		define BHAMT_NODE_TYPE(ROOT) typeof(ROOT)
#	else
#		define BHAMT_NODE_TYPE(ROOT) __typeof__(ROOT)
#	endif
#endif

#define BHAMT_NUM_CHILDREN (1 << BHAMT_NUM_BITS)
#define BHAMT_MASK (((BHAMT_HASH_TYPE)1 << BHAMT_NUM_BITS) - 1)

#define BHAMT_SEARCH(ROOT, ITR, RESULT, FREE_NODE, HASH, KEY) \
	do { \
		ITR = &(ROOT); \
		FREE_NODE = NULL; \
		RESULT = NULL; \
		for ( \
			BHAMT_HASH_TYPE hamt__itr_hash = (HASH); \
			*ITR != NULL; \
			hamt__itr_hash >>= BHAMT_NUM_BITS \
		) { \
			if (BHAMT_IS_TOMBSTONE((*ITR))) { \
				FREE_NODE = *ITR; \
			} else if (BHAMT_KEYEQ(((*ITR)->key), KEY)) { \
				RESULT = *ITR; \
				break; \
			} \
			ITR = &(*ITR)->children[hamt__itr_hash & BHAMT_MASK]; \
		} \
	} while (0)

#define BHAMT_GET(ROOT, RESULT, HASH, KEY) \
	do { \
		BHAMT_NODE_TYPE(ROOT) bhamt__free_node; \
		(void)bhamt__free_node; \
		BHAMT_NODE_TYPE(ROOT)* bhamt__itr; \
		BHAMT_SEARCH(ROOT, bhamt__itr, RESULT, bhamt__free_node, HASH, KEY); \
	} while (0)

#endif
