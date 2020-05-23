#include "record.h"
#define XXH_INLINE_ALL
#include <xxHash/xxhash.h>


static inline bool
mara_try_record_seed(
	mara_hash_t seed,
	int num_attrs,
	struct mara_record_attr_info_s* attr_infos
)
{
	mara_string_t* slots[num_attrs];
	memset(slots, 0, sizeof(mara_string_t) * num_attrs);

	for(int i = 0; i < num_attrs; ++i)
	{
		mara_string_t* name = attr_infos[i].name;
		mara_hash_t name_hash = MARA_HASH(seed, name, sizeof(name));
		int index = name_hash % num_attrs;

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


mara_record_info_t*
mara_create_record_info(
	mara_context_t* ctx,
	mara_string_t* name,
	int num_attrs,
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
	mara_gc_register(ctx, &record_info->gc_header);

	record_info->name = name;
	record_info->num_attrs = num_attrs;
	struct mara_record_attr_info_s* attr_infos = record_info->attr_infos;

	for(int i = 0; i < num_attrs; ++i)
	{
		attr_infos[i].name = attributes[i];

		for(int j = i - 1; j >= 0; --j)
		{
			MARA_ASSERT(
				ctx,
				attr_infos[j].name != attr_infos[i].name,
				"Duplicate record attributes"
			);
		}
	}

	bool found_solution = false;

	for(int i = 0; i < MARA_STACK_MAX; ++i)
	{
		mara_hash_t seed = MARA_HASH(i, name, sizeof(name));
		if(mara_try_record_seed(seed, num_attrs, attr_infos))
		{
			record_info->seed = seed;
			break;
		}
	}

	MARA_ASSERT(ctx, found_solution, "Could not perfectly hash record's attributes");

	return record_info;
}

void
mara_declare_record(mara_context_t* ctx, const mara_record_decl_t* decl)
{
	int num_attrs = MARA_RECORD_ATTR_MAX + 1;

	for(int i = 0; i < MARA_RECORD_ATTR_MAX + 1; ++i)
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

		for(int i = 0; i < num_attrs; ++i)
		{
			mara_push_symbol(ctx, decl->attributes[i]);
			attributes[i] = MARA_AS_GC_TYPE(ctx, -1, mara_string_t);
		}

		mara_record_info_t* record_info = mara_create_record_info(
			ctx, name, num_attrs, attributes
		);
		mara_push_gc_obj(ctx, &record_info->gc_header);
		mara_set_user_data(ctx, decl, -1);
	}
	mara_restore_stack(ctx, stack);
}
