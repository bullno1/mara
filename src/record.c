#include "record.h"
#include "ptr_map.h"
#define XXH_INLINE_ALL
#include <xxHash/xxhash.h>


static inline bool
mara_try_record_seed(
	mara_hash_t seed,
	mara_index_t num_attrs,
	struct mara_record_attr_info_s* attr_infos
)
{
	mara_string_t* slots[num_attrs];
	memset(slots, 0, sizeof(mara_string_t) * num_attrs);

	for(mara_index_t i = 0; i < num_attrs; ++i)
	{
		mara_string_t* name = attr_infos[i].name;
		mara_hash_t name_hash = MARA_HASH(seed, name, sizeof(name));
		mara_index_t index = name_hash % num_attrs;

		if(slots[index] == NULL)
		{
			slots[index] = name;
			attr_infos[index].slot = i;
		}
		else
		{
			return false;
		}
	}

	return true;
}

static mara_record_info_t*
mara_get_record_info(mara_context_t* ctx, const mara_record_decl_t* decl)
{
	mara_value_t record_info_v = mara_ptr_map_get(
		ctx, &ctx->record_decls, decl
	);
	MARA_ASSERT(
		ctx,
		mara_value_type_check(&record_info_v, MARA_VAL_GC_OBJ),
		"Unknown record type"
	);

	mara_gc_header_t* gc_header = mara_value_as_gc_obj(&record_info_v);
	MARA_ASSERT(
		ctx,
		gc_header->obj_type == MARA_GC_RECORD_INFO,
		"Sum Ting Wong"
	);

	return BK_CONTAINER_OF(gc_header, mara_record_info_t, gc_header);
}


mara_record_info_t*
mara_create_record_info(
	mara_context_t* ctx,
	mara_string_t* name,
	mara_index_t num_attrs,
	mara_string_t* attributes[]
)
{
	MARA_ASSERT(
		ctx,
		num_attrs <= MARA_RECORD_ATTR_MAX,
		"Record has too many attributes"
	);

	mara_record_info_t* record_info = mara_malloc(
		ctx,
		sizeof(mara_record_info_t) +
		num_attrs * sizeof(struct mara_record_attr_info_s)
	);
	record_info->gc_header.obj_type = MARA_GC_RECORD_INFO;

	record_info->name = name;
	record_info->num_attrs = num_attrs;
	struct mara_record_attr_info_s* attr_infos = record_info->attr_infos;

	for(mara_index_t i = 0; i < num_attrs; ++i)
	{
		attr_infos[i].name = attributes[i];

		for(mara_index_t j = i - 1; j >= 0; --j)
		{
			MARA_ASSERT(
				ctx,
				attr_infos[j].name != attr_infos[i].name,
				"Duplicate record attributes"
			);
		}
	}

	bool found_solution = false;

	for(mara_index_t i = 0; i < MARA_STACK_MAX; ++i)
	{
		mara_hash_t seed = MARA_HASH(i, name, sizeof(name));
		if(mara_try_record_seed(seed, num_attrs, attr_infos))
		{
			record_info->seed = seed;
			break;
		}
	}

	MARA_ASSERT(ctx, found_solution, "Could not perfectly hash record's attributes");

	mara_gc_register(ctx, &record_info->gc_header);
	return record_info;
}

void
mara_declare_record(mara_context_t* ctx, const mara_record_decl_t* decl)
{
	mara_index_t num_attrs = MARA_RECORD_ATTR_MAX + 1;

	for(mara_index_t i = 0; i < MARA_RECORD_ATTR_MAX + 1; ++i)
	{
		if(decl->attributes[i].ptr == NULL)
		{
			num_attrs = i;
			break;
		}
	}

	MARA_ASSERT(
		ctx,
		num_attrs <= MARA_RECORD_ATTR_MAX,
		"Record has too many attributes"
	);

	mara_string_t* attributes[num_attrs];
	mara_index_t stack = mara_stack_len(ctx);
	{
		mara_push_symbol(ctx, decl->name);
		mara_string_t* name = MARA_AS_GC_TYPE(ctx, -1, mara_string_t);

		for(mara_index_t i = 0; i < num_attrs; ++i)
		{
			mara_push_symbol(ctx, decl->attributes[i]);
			attributes[i] = MARA_AS_GC_TYPE(ctx, -1, mara_string_t);
		}

		mara_record_info_t* record_info = mara_create_record_info(
			ctx, name, num_attrs, attributes
		);
		mara_push_gc_obj(ctx, &record_info->gc_header);
		mara_ptr_map_set(
			ctx,
			&ctx->record_decls,
			decl,
			mara_stack_addr(ctx, -1)
		);
	}
	mara_restore_stack(ctx, stack);
}

void
mara_new_record(mara_context_t* ctx, const mara_record_decl_t* decl)
{
	mara_record_info_t* record_info = mara_get_record_info(ctx, decl);

	mara_index_t stack_len = mara_stack_len(ctx);
	MARA_ASSERT(
		ctx,
		stack_len >= record_info->num_attrs,
		"Not enough arguments"
	);

	mara_record_t* record = mara_malloc(
		ctx, sizeof(mara_record_t) + record_info->num_attrs * sizeof(mara_value_t)
	);
	record->gc_header.obj_type = MARA_GC_RECORD;
	record->record_info = record_info;
	for(mara_index_t i = 0; i < record_info->num_attrs; ++i)
	{
		record->attributes[i] = *mara_stack_addr(
			ctx, i - record_info->num_attrs
		);
	}

	mara_gc_register(ctx, &record->gc_header);
	mara_push_gc_obj(ctx, &record->gc_header);
	mara_replace(ctx, -1 - record_info->num_attrs);
	mara_restore_stack(ctx, stack_len - record_info->num_attrs + 1);
}

const mara_record_decl_t*
mara_get_record_decl(mara_context_t* ctx, mara_index_t index)
{
	mara_value_t* value = mara_stack_addr(ctx, index);
	if(!mara_value_type_check(value, MARA_VAL_GC_OBJ)) { return NULL; }

	mara_gc_header_t* gc_header = mara_value_as_gc_obj(value);
	if(gc_header->obj_type != MARA_GC_RECORD) { return NULL; }

	mara_record_t* record = BK_CONTAINER_OF(gc_header, mara_record_t, gc_header);
	return record->record_info->decl;
}

void
mara_get_attribute_name(
	mara_context_t* ctx,
	const mara_record_decl_t* decl,
	mara_index_t attribute_index
)
{
	mara_record_info_t* record_info = mara_get_record_info(ctx, decl);
	MARA_ASSERT(
		ctx,
		attribute_index < record_info->num_attrs,
		"Attribute index is out of bound"
	);

	mara_push_gc_obj(
		ctx, &record_info->attr_infos[attribute_index].name->gc_header
	);
}

MARA_DECL mara_index_t
mara_get_attribute_index(
	mara_context_t* ctx,
	const mara_record_decl_t* decl,
	mara_index_t attribute_name
)
{
	mara_record_info_t* record_info = mara_get_record_info(ctx, decl);

	mara_value_t* value = mara_stack_addr(ctx, attribute_name);
	MARA_ASSERT(
		ctx, mara_value_type_check(value, MARA_VAL_GC_OBJ), "Expecting symbol"
	);

	mara_gc_header_t* gc_header = mara_value_as_gc_obj(value);
	MARA_ASSERT(
		ctx, gc_header->obj_type == MARA_GC_SYMBOL, "Expecting symbol"
	);

	mara_string_t* attr_name = BK_CONTAINER_OF(
		gc_header, mara_string_t, gc_header
	);

	mara_index_t hash = MARA_HASH(
		record_info->seed, attr_name, sizeof(attr_name)
	);
	mara_index_t info_slot = hash % record_info->num_attrs;
	mara_index_t data_slot = record_info->attr_infos[info_slot].slot;
	bool correct_name = record_info->attr_infos[data_slot].name == attr_name;

	return correct_name ? data_slot : -1;
}

MARA_DECL void
mara_record_get(
	mara_context_t* ctx,
	mara_index_t record,
	mara_index_t attribute_index
)
{
}

MARA_DECL void
mara_record_set(
	mara_context_t* ctx,
	mara_index_t record,
	mara_index_t attribute_index
)
{
}
