#include "internal.h"

#if !MARA_NANBOX

void
mara_value_set_null(mara_value_t* value)
{
	value->type = MARA_VAL_NULL;
}

void
mara_value_set_number(mara_value_t* value, mara_number_t num)
{
	value->type = MARA_VAL_NUMBER;
	value->data.number = num;
}

void
mara_value_set_bool(mara_value_t* value, bool boolean)
{
	value->type = MARA_VAL_BOOL;
	value->data.boolean = boolean;
}

void
mara_value_set_gc_obj(mara_value_t* value, mara_gc_header_t* obj)
{
	value->type = MARA_VAL_GC_OBJ;
	value->data.gc_obj = obj;
}

bool
mara_value_type_check(mara_value_t* value, mara_value_type_t type)
{
	return value->type == type;
}

mara_number_t
mara_value_as_number(mara_value_t* value)
{
	return value->data.number;
}

bool
mara_value_as_bool(mara_value_t* value)
{
	return value->data.boolean;
}

mara_gc_header_t*
mara_value_as_gc_obj(mara_value_t* value)
{
	return value->data.gc_obj;
}

#endif
