#include "ptr_map.h"
#include "vendor/robinhoodhash.h"
#define XXH_INLINE_ALL
#include <xxHash/xxhash.h>


#define mara_ptr_map_setvalue(u, index, key, value) \
	do { \
		u->keys[index] = key; \
		u->values[index] = value; \
	} while(0);
#define mara_ptr_map_setnil(u, index) mara_value_set_null(&u->values[index]);
#define mara_ptr_map_getkey(u, index) u->keys[index]
#define mara_ptr_map_getvalue(u, index) u->values[index]
#define mara_ptr_map_keysequal(u, key1, key2) key1 == key2
#define mara_ptr_map_isnil(u, index) \
	mara_value_type_check(&u->values[index], MARA_VAL_NULL)
#define mara_ptr_map_n_elem(u) u->capacity + 1
#define mara_ptr_map_size(u) u->size
#define mara_ptr_map_overflow(u) MARA_ASSERT(ctx, 0, "Hash overflow")
#define mara_ptr_map_removefailed(u, key)
#define mara_ptr_map_swap(u, index1, index2) \
	do { \
		MARA_HASH_SWAP(mara_ptr_map_key_t, u->keys, index1, index2); \
		MARA_HASH_SWAP(mara_value_t, u->values, index1, index2); \
	} while(0)


static inline mara_value_t
mara_ptr_map_nilvalue(mara_ptr_map_t* map)
{
	(void)map;
	mara_value_t result;
	mara_value_set_null(&result);
	return result;
}

static inline mara_hash_t
mara_ptr_map_getbucket(
	mara_ptr_map_t* map,
	mara_ptr_map_key_t key
)
{
	return (MARA_HASH(map, &key, sizeof(key)) & (map->capacity - 1)) + 1;
}

static void
mara_ptr_map_init_internal(
	mara_context_t* ctx,
	mara_ptr_map_t* map,
	size_t capacity
)
{
	map->keys = mara_malloc(ctx, (capacity + 1) * sizeof(mara_ptr_map_key_t));
	map->values = mara_malloc(ctx, (capacity + 1) * sizeof(mara_value_t));
	map->size = 0;
	map->capacity = capacity;

	ROBINHOOD_HASH_CLEAR(mara_ptr_map, map);
}


void
mara_ptr_map_init(mara_context_t* ctx, mara_ptr_map_t* map)
{
	mara_ptr_map_init_internal(ctx, map, MARA_HASH_INITIAL_CAPACITY);
}

void
mara_ptr_map_cleanup(mara_context_t* ctx, mara_ptr_map_t* map)
{
	mara_free(ctx, map->keys);
	mara_free(ctx, map->values);
}

mara_value_t
mara_ptr_map_get(
	mara_context_t* ctx,
	mara_ptr_map_t* map,
	mara_ptr_map_key_t ptr
);

void
mara_ptr_map_set(
	mara_context_t* ctx,
	mara_ptr_map_t* map,
	mara_ptr_map_key_t ptr,
	mara_value_t* value
)
{
	// Set value to null is treated as delete
	if(mara_value_type_check(value, MARA_VAL_NULL))
	{
		mara_ptr_map_delete(ctx, map, ptr);
		return;
	}

	size_t capacity = map->capacity;
	size_t max_size = (size_t)((double)capacity * MARA_HASH_LOAD_FACTOR);

	if(map->size >= max_size)
	{
		mara_ptr_map_t new_map;
		mara_ptr_map_init_internal(ctx, &new_map, capacity * 2);

		ROBINHOOD_HASH_FOREACH(mara_ptr_map, map, i)
		{
			ROBINHOOD_HASH_SET(
				mara_ptr_map,
				(&new_map),
				map->keys[i],
				map->values[i]
			);
		}

		mara_ptr_map_cleanup(ctx, map);
		*map = new_map;
	}

	ROBINHOOD_HASH_SET(mara_ptr_map, map, ptr, *value);
}

void
mara_ptr_map_delete(
	mara_context_t* ctx,
	mara_ptr_map_t* map,
	mara_ptr_map_key_t ptr
)
{
	(void)ctx;
	ROBINHOOD_HASH_DEL(mara_ptr_map, map, ptr);
}

void
mara_gc_mark_ptr_map(mara_context_t* ctx, mara_ptr_map_t* map)
{
	for(size_t i = 1; i < map->capacity + 1; ++i)
	{
		mara_value_t* value = &map->values[i];
		if(mara_value_type_check(value, MARA_VAL_GC_OBJ))
		{
			mara_gc_mark(ctx, mara_value_as_gc_obj(value));
		}
	}
}
