#include "internal.h"

mara_value_t
mara_copy(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_value_t value) {
	mara_obj_t* obj = mara_value_to_obj(value);
	if (obj == NULL || obj->zone == zone) { return value; }

	(void)ctx;
	return mara_null();
}
