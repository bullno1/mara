#include "internal.h"

mara_error_t*
mara_copy(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_value_t value, mara_value_t* result) {
	mara_obj_t* obj = mara_value_to_obj(value);
	if (
		obj == NULL
		|| (
			obj->zone->branch == zone->branch
			&& obj->zone->level <= zone->level
		)
	) {
		*result = value;
		return NULL;
	}

	switch (obj->type) {
		case MARA_OBJ_TYPE_STRING:
			*result = mara_new_str(ctx, zone, *(mara_str_t*)obj->body);
			return NULL;
		case MARA_OBJ_TYPE_SYMBOL:
			*result = value;
			return NULL;
		case MARA_OBJ_TYPE_REF:
			{
				mara_obj_ref_t* ref = (mara_obj_ref_t*)obj->body;
				*result = mara_new_ref(ctx, zone, ref->tag, ref->value);
				return NULL;
			}
		case MARA_OBJ_TYPE_LIST:
			{
				mara_obj_list_t* old_list;
				mara_check_error(mara_unbox_list(ctx, value, &old_list));

				mara_value_t new_list = mara_new_list(ctx, zone, old_list->len);
				mara_obj_list_t* new_list_obj;
				mara_check_error(mara_unbox_list(ctx, new_list, &new_list_obj));

				mara_index_t len = new_list_obj->len = old_list->len;
				mara_value_t* old_elems = old_list->elems;
				mara_value_t* new_elems = new_list_obj->elems;
				for (mara_index_t i = 0; i < len; ++i) {
					mara_check_error(mara_copy(ctx, zone, old_elems[i], &new_elems[i]));
				}

				*result = new_list;
				return NULL;
			}
		default:
			return mara_errorf(
				ctx,
				mara_str_from_literal("core/not-implemented"),
				"Not implemented",
				mara_null()
			);
	}
}
