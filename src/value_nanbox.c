#include "internal.h"

#if MARA_NANBOX

#define MARA_NANBOX_MASK            0x7FF4000000000000ull
#define MARA_NANBOX_QNAN_MASK       0x7FF8000000000000ull
#define MARA_NANBOX_NULL_TAG        0x0ull
#define MARA_NANBOX_BOOL_TAG        0x1ull
#define MARA_NANBOX_GC_OBJ_TAG      0x2ull
#define MARA_NANBOX_TAG_MASK        0x0003000000000000ull
#define MARA_NANBOX_TAG_SHIFT       48
#define MARA_NANBOX_PAYLOAD_MASK    0x0000FFFFFFFFFFFFull


static inline uint64_t
mara_nanbox_create(uint64_t tag, uint64_t payload)
{
	return MARA_NANBOX_MASK
		| (tag << MARA_NANBOX_TAG_SHIFT)
		| payload;
}

static inline uint64_t
mara_nanbox_tag(mara_value_t value)
{
	return (value.u64 & MARA_NANBOX_TAG_MASK) >> MARA_NANBOX_TAG_SHIFT;
}

static inline uint64_t
mara_nanbox_payload(mara_value_t value)
{
	return value.u64 & MARA_NANBOX_PAYLOAD_MASK;
}

static inline bool
mara_nanbox_check_mask(mara_value_t value, uint64_t mask)
{
	return (value.u64 & mask) == mask;
}

static inline bool
mara_value_is_nanbox(mara_value_t value)
{
	return mara_nanbox_check_mask(value, MARA_NANBOX_MASK)
		&& !mara_nanbox_check_mask(value, MARA_NANBOX_QNAN_MASK);
}


bool
mara_value_type_check(mara_value_t* value, mara_value_type_t type)
{
	switch(type)
	{
		case MARA_VAL_NUMBER:
			return !mara_value_is_nanbox(*value);
		case MARA_VAL_NULL:
			return mara_value_is_nanbox(*value)
				&& mara_nanbox_tag(*value) == MARA_NANBOX_NULL_TAG;
		case MARA_VAL_BOOL:
			return mara_value_is_nanbox(*value)
				&& mara_nanbox_tag(*value) == MARA_NANBOX_BOOL_TAG;
		case MARA_VAL_GC_OBJ:
			return mara_value_is_nanbox(*value)
				&& mara_nanbox_tag(*value) == MARA_NANBOX_GC_OBJ_TAG;
	}

	return false;
}

mara_gc_header_t*
mara_value_as_gc_obj(mara_value_t* value)
{
	return (void*)mara_nanbox_payload(*value);
}

mara_number_t
mara_value_as_number(mara_value_t* value)
{
	return value->d64;
}

bool
mara_value_as_bool(mara_value_t* value)
{
	return mara_nanbox_payload(*value);
}

void
mara_value_set_null(mara_value_t* value)
{
	value->u64 = mara_nanbox_create(MARA_NANBOX_NULL_TAG, 0);
}

void
mara_value_set_number(mara_value_t* value, mara_number_t num)
{
	value->d64 = num;
}

void
mara_value_set_bool(mara_value_t* value, bool boolean)
{
	value->u64 = mara_nanbox_create(MARA_NANBOX_BOOL_TAG, boolean);
}

void
mara_value_set_gc_obj(mara_value_t* value, mara_gc_header_t* ptr)
{
	uint64_t payload = (uint64_t)ptr;
	BK_ASSERT(
		(payload & MARA_NANBOX_PAYLOAD_MASK) == payload,
		"Invalid pointer for nan-boxing"
	);
	value->u64 = mara_nanbox_create(MARA_NANBOX_GC_OBJ_TAG, payload);
}

#endif
