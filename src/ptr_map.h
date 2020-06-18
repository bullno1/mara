#ifndef MARA_PTR_MAP
#define MARA_PTR_MAP

#include "internal.h"


void
mara_ptr_map_init(mara_context_t* ctx, mara_ptr_map_t* map);

void
mara_ptr_map_cleanup(mara_context_t* ctx, mara_ptr_map_t* map);

mara_value_t
mara_ptr_map_get(
	mara_context_t* ctx,
	mara_ptr_map_t* map,
	const void* ptr
);

void
mara_ptr_map_set(
	mara_context_t* ctx,
	mara_ptr_map_t* map,
	const void* ptr,
	mara_value_t* value
);

void
mara_ptr_map_delete(
	mara_context_t* ctx,
	mara_ptr_map_t* map,
	mara_ptr_map_key_t ptr
);

void
mara_gc_mark_ptr_map(mara_context_t* ctx, mara_ptr_map_t* map);

#endif
