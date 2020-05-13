#include "internal.h"

#if MARA_NANBOX

#define MARA_NANBOX_MASK            0x7FF0000000000001ull
#define MARA_QNAN_MASK              0x7FF8000000000000ull
#define MARA_NANBOX_BOOL_TAG        0x0ull
#define MARA_NANBOX_POINTER_TAG     0x1ull
#define MARA_NANBOX_POINTER_MASK    0x0000FFFFFFFFFFF2ull

#define MARA_MAKE_NANBOX(TAG, PAYLOAD)  (((TAG) << 48) | ((PAYLOAD) << 1) | MARA_NANBOX_MASK)
#define MARA_NANBOX_NULL        MARA_MAKE_NANBOX(MARA_NANBOX_POINTER_TAG, 0)
#define MARA_NANBOX_FALSE       MARA_MAKE_NANBOX(MARA_NANBOX_BOOL_TAG, 0)
#define MARA_NANBOX_TRUE        MARA_MAKE_NANBOX(MARA_NANBOX_BOOL_TAG, 1)


static inline bool
mara_value_is_nanbox(mara_value_t value)
{
	return (value.u64 & MARA_NANBOX_MASK) == MARA_NANBOX_MASK
		&& (value.u64 & MARA_QNAN_MASK) != MARA_QNAN_MASK;
}

static inline bool
mara_value_type_check_ptr(mara_value_t value, mara_gc_obj_type_t type)
{
	mara_gc_header_t* header = mara_value_as_ptr(value);
	return  header->obj_type == type;
}

bool
mara_value_type_check(mara_value_t value, mara_value_type_t type)
{
#define MARA_NANBOX_TRIVIAL_PTR_TYPE(X) \
	X(STRING) \
	X(SYMBOL) \
	X(LIST) \
	X(HANDLE) \
	X(THREAD) \

#define MARA_NANBOX_TRIVIAL_PTR_CHECK(X) \
	case MARA_PP_CONCAT(MARA_VAL_, X): \
		return mara_value_is_nanbox(value) \
			&& mara_value_type_check_ptr(value, MARA_PP_CONCAT(MARA_GC_, X));

	switch(type)
	{
		case MARA_VAL_NULL:
			return value.u64 == MARA_NANBOX_NULL;
		case MARA_VAL_BOOL:
			return value.u64 == MARA_NANBOX_TRUE || value.u64 == MARA_NANBOX_FALSE;
		case MARA_VAL_NUMBER:
			return !mara_value_is_nanbox(value);
		case MARA_VAL_FUNCTION:
			return mara_value_is_nanbox(value) \
				&& mara_value_type_check_ptr(value, MARA_GC_FUNCTION_CLOSURE);
		MARA_NANBOX_TRIVIAL_PTR_TYPE(MARA_NANBOX_TRIVIAL_PTR_CHECK)
	}

	return false;
}

mara_gc_header_t*
mara_value_as_ptr(mara_value_t value)
{
	return (void*)(value.u64 & MARA_NANBOX_POINTER_MASK);
}

mara_number_t
mara_value_as_number(mara_value_t value)
{
	return value.d64;
}

bool
mara_value_as_bool(mara_value_t value)
{
	return value.u64 == MARA_NANBOX_TRUE;
}

void
mara_value_set_null(mara_value_t* value)
{
	value->u64 = MARA_NANBOX_NULL;
}

void
mara_value_set_number(mara_value_t* value, mara_number_t num)
{
	value->d64 = num;
}

void
mara_value_set_bool(mara_value_t* value, bool boolean)
{
	value->u64 = MARA_MAKE_NANBOX(MARA_NANBOX_BOOL_TAG, boolean);
}

void
mara_value_set_ptr(
	mara_value_t* value, mara_value_type_t type, mara_gc_header_t* ptr
)
{
	(void)type;
	value->u64 = MARA_MAKE_NANBOX(MARA_NANBOX_POINTER_TAG, ((uint64_t)ptr >> 1));
}

#endif
