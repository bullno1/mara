#include "internal.h"

mara_error_t*
mara_copy(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_value_t value, mara_value_t* result) {
	mara_obj_t* obj = mara_value_to_obj(value);
	if (
		obj == NULL
		|| obj->zone->level <= zone->level
	) {
		*result = value;
		return NULL;
	}

	return mara_errorf(
		ctx,
		mara_str_from_literal("core/not-implemented"),
		"Not implemented",
		mara_null()
	);
}
