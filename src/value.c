#include "internal.h"
#include "vendor/nanbox.h"
#include "vendor/nanoprintf.h"

_Static_assert(sizeof(nanbox_t) == sizeof(mara_value_t), "mara_value_t cannot be nan-boxed");

mara_error_t*
mara_type_error(
	mara_exec_ctx_t* ctx,
	mara_value_type_t expected,
	mara_value_t value
) {
	return mara_errorf(
		ctx,
		mara_str_from_literal("core/unexpected-type"),
		"Expecting %s got %s",
		mara_nil(),
		mara_value_type_name(expected),
		mara_value_type_name(mara_value_type(value, NULL))
	);
}

mara_obj_t*
mara_alloc_obj(mara_exec_ctx_t* ctx, mara_zone_t* zone, size_t size) {
	mara_obj_t* obj = mara_zone_alloc(ctx, zone, sizeof(mara_obj_t) + size);
	mara_assert(obj != NULL, "Out of memory");
	obj->zone = zone;
	if (
		MARA_EXPECT(
			ctx->arenas <= zone->arena
			&& zone->arena < ctx->arenas + MARA_NUM_ARENAS
		)
	) {
		obj->arena_mask = mara_arena_mask_of_zone(ctx, zone);
	}
	return obj;
}

mara_obj_t*
mara_value_to_obj(mara_value_t value) {
	nanbox_t nanbox = { .as_int64 = value };
	if (nanbox_is_pointer(nanbox)) {
		return nanbox_to_pointer(nanbox);
	} else {
		return NULL;
	}
}

mara_value_t
mara_obj_to_value(mara_obj_t* obj) {
	return nanbox_from_pointer(obj).as_int64;
}

bool
mara_value_is_nil(mara_value_t value) {
	nanbox_t nanbox = { .as_int64 = value };
	return nanbox_is_null(nanbox);
}

bool
mara_value_is_int(mara_value_t value) {
	nanbox_t nanbox = { .as_int64 = value };
	return nanbox_is_int(nanbox);
}

bool
mara_value_is_real(mara_value_t value) {
	nanbox_t nanbox = { .as_int64 = value };
	return nanbox_is_double(nanbox);
}

bool
mara_value_is_bool(mara_value_t value) {
	nanbox_t nanbox = { .as_int64 = value };
	return nanbox_is_boolean(nanbox);
}

bool
mara_value_is_str(mara_value_t value) {
	mara_obj_t* obj = mara_value_to_obj(value);
	return obj != NULL && obj->type == MARA_OBJ_TYPE_STRING;
}

bool
mara_value_is_symbol(mara_value_t value) {
	nanbox_t nanbox = { .as_int64 = value };
	return nanbox_is_aux(nanbox) && nanbox.as_bits.tag == NANBOX_MIN_AUX_TAG;
}

bool
mara_value_is_ref(mara_value_t value, void* tag) {
	mara_obj_t* obj = mara_value_to_obj(value);
	return obj != NULL
		&& obj->type == MARA_OBJ_TYPE_REF
		&& ((mara_obj_ref_t*)obj->body)->tag == tag;
}

bool
mara_value_is_function(mara_value_t value) {
	mara_obj_t* obj = mara_value_to_obj(value);
	return obj != NULL
		&& (
			obj->type == MARA_OBJ_TYPE_MARA_FN
			|| obj->type == MARA_OBJ_TYPE_NATIVE_FN
		);
}

bool
mara_value_is_list(mara_value_t value) {
	mara_obj_t* obj = mara_value_to_obj(value);
	return obj != NULL && obj->type == MARA_OBJ_TYPE_LIST;
}

bool
mara_value_is_map(mara_value_t value) {
	mara_obj_t* obj = mara_value_to_obj(value);
	return obj != NULL && obj->type == MARA_OBJ_TYPE_MAP;
}

mara_value_type_t
mara_value_type(mara_value_t value, void** tag) {
	nanbox_t nanbox = { .as_int64 = value };
	if (nanbox_is_null(nanbox)) {
		return MARA_VAL_NIL;
	} else if (nanbox_is_int(nanbox)) {
		return MARA_VAL_INT;
	} else if (nanbox_is_double(nanbox)) {
		return MARA_VAL_REAL;
	} else if (nanbox_is_boolean(nanbox)) {
		return MARA_VAL_BOOL;
	} else if (
		nanbox_is_aux(nanbox)
		&& nanbox.as_bits.tag == NANBOX_MIN_AUX_TAG
	) {
		return MARA_VAL_SYMBOL;
	} else if (nanbox_is_pointer(nanbox)) {
		mara_obj_t* obj = nanbox_to_pointer(nanbox);
		switch (obj->type) {
			case MARA_OBJ_TYPE_STRING:
				return MARA_VAL_STRING;
			case MARA_OBJ_TYPE_REF:
				if (tag != NULL) {
					*tag = ((mara_obj_ref_t*)obj->body)->tag;
				}
				return MARA_VAL_REF;
			case MARA_OBJ_TYPE_MARA_FN:
			case MARA_OBJ_TYPE_NATIVE_FN:
				return MARA_VAL_FUNCTION;
			case MARA_OBJ_TYPE_LIST:
				return MARA_VAL_LIST;
			case MARA_OBJ_TYPE_MAP:
				return MARA_VAL_MAP;
			default:
				mara_assert(false, "Corrupted value");
				return MARA_VAL_NIL;
		}
	} else {
		mara_assert(false, "Corrupted value");
		return MARA_VAL_NIL;
	}
}

mara_error_t*
mara_value_to_int(mara_exec_ctx_t* ctx, mara_value_t value, int* result) {
	nanbox_t nanbox = { .as_int64 = value };
	if (MARA_EXPECT(nanbox_is_int(nanbox))) {
		*result = nanbox_to_int(nanbox);
		return NULL;
	} else {
		return mara_type_error(ctx, MARA_VAL_INT, value);
	}
}

mara_error_t*
mara_value_to_real(mara_exec_ctx_t* ctx, mara_value_t value, double* result) {
	nanbox_t nanbox = { .as_int64 = value };
	if (MARA_EXPECT(nanbox_is_double(nanbox))) {
		*result = nanbox_to_double(nanbox);
		return NULL;
	} else {
		return mara_type_error(ctx, MARA_VAL_REAL, value);
	}
}

mara_error_t*
mara_value_to_bool(mara_exec_ctx_t* ctx, mara_value_t value, bool* result) {
	nanbox_t nanbox = { .as_int64 = value };
	if (MARA_EXPECT(nanbox_is_boolean(nanbox))) {
		*result = nanbox_to_boolean(nanbox);
		return NULL;
	} else {
		return mara_type_error(ctx, MARA_VAL_BOOL, value);
	}
}

mara_error_t*
mara_value_to_str(mara_exec_ctx_t* ctx, mara_value_t value, mara_str_t* result) {
	if (mara_value_is_str(value)) {
		mara_obj_t* obj = mara_value_to_obj(value);
		*result = *(mara_str_t*)obj->body;
		return NULL;
	} else if (mara_value_is_symbol(value)) {
		nanbox_t nanbox = { .as_int64 = value };
		*result = mara_strpool_lookup(&ctx->env->symtab, nanbox.as_bits.payload);
		return NULL;
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/unexpected-type"),
			"Expecting %s got %s",
			mara_nil(),
			"string or symbol",
			mara_value_type_name(mara_value_type(value, NULL))
		);
	}
}

mara_error_t*
mara_value_to_ref(mara_exec_ctx_t* ctx, mara_value_t value, void* tag, void** result) {
	if (MARA_EXPECT(mara_value_is_ref(value, tag))) {
		mara_obj_t* obj = mara_value_to_obj(value);
		*result = ((mara_obj_ref_t*)obj->body)->value;
		return NULL;
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/unexpected-type"),
			"Expecting %s:%p got %s",
			mara_nil(),
			"ref", tag,
			mara_value_type_name(mara_value_type(value, NULL))
		);
	}
}

mara_value_t
mara_nil(void) {
	return nanbox_null().as_int64;
}

mara_value_t
mara_value_from_bool(bool value) {
	return nanbox_from_boolean(value).as_int64;
}

mara_value_t
mara_value_from_int(mara_index_t value) {
	return nanbox_from_int(value).as_int64;
}

mara_value_t
mara_value_from_real(double value) {
	return nanbox_from_double(value).as_int64;
}

mara_str_t
mara_vsnprintf(mara_exec_ctx_t* ctx, mara_zone_t* zone, const char* fmt, va_list args) {
	va_list args_copy;
	va_copy(args_copy, args);
	char buf[512];
	int len = npf_vsnprintf(buf, sizeof(buf), fmt, args_copy);
	if (len < 0) { len = 0; }

	char* chars = mara_zone_alloc_ex(ctx, zone, (size_t)len, _Alignof(char));

	if ((size_t)len < sizeof(buf)) {
		memcpy(chars, buf, (size_t)len);
	} else {
		npf_vsnprintf(chars, (size_t)len, fmt, args);
	}

	return (mara_str_t){
		.len = (size_t)len,
		.data = chars,
	};
}

mara_value_t
mara_new_str(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_str_t value) {
	mara_obj_t* obj = mara_alloc_obj(ctx, zone, sizeof(mara_str_t) + value.len);
	obj->type = MARA_OBJ_TYPE_STRING;

	mara_str_t* str = (mara_str_t*)obj->body;
	str->len = value.len;
	char* chars = (char*)obj + sizeof(mara_str_t);
	str->data = chars;
	memcpy(chars, value.data, value.len);

	return mara_obj_to_value(obj);
}

mara_value_t
mara_new_strf(mara_exec_ctx_t* ctx, mara_zone_t* zone, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	mara_value_t result = mara_new_strv(ctx, zone, fmt, args);
	va_end(args);
	return result;
}

mara_value_t
mara_new_strv(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	const char* fmt,
	va_list args
) {
	mara_obj_t* obj = mara_alloc_obj(ctx, zone, sizeof(mara_str_t));
	obj->type = MARA_OBJ_TYPE_STRING;

	mara_str_t* str = (mara_str_t*)obj->body;
	*str = mara_vsnprintf(ctx, zone, fmt, args);

	return mara_obj_to_value(obj);
}

mara_value_t
mara_new_ref(mara_exec_ctx_t* ctx, mara_zone_t* zone, void* tag, void* value) {
	mara_obj_t* obj = mara_alloc_obj(ctx, zone, sizeof(mara_obj_ref_t));
	obj->type = MARA_OBJ_TYPE_REF;

	mara_obj_ref_t* ref = (mara_obj_ref_t*)obj->body;
	ref->tag = tag;
	ref->value = value;

	return mara_obj_to_value(obj);
}

mara_value_t
mara_tombstone(void) {
	return nanbox_deleted().as_int64;
}

bool
mara_value_is_tombstone(mara_value_t value) {
	return nanbox_is_deleted((nanbox_t){ .as_int64 = value });
}

bool
mara_value_is_true(mara_value_t value) {
	return nanbox_is_true((nanbox_t){ .as_int64 = value });
}

bool
mara_value_is_false(mara_value_t value) {
	return nanbox_is_false((nanbox_t){ .as_int64 = value });
}

mara_value_t
mara_new_symbol(mara_exec_ctx_t* ctx, mara_str_t name) {
	mara_env_t* env = ctx->env;
	mara_index_t id = mara_strpool_intern(&env->symtab, name);

	nanbox_t nanbox = {
		.as_bits = { .payload = id, .tag = NANBOX_MIN_AUX_TAG },
	};
	return nanbox.as_int64;
}

void
mara_obj_add_arena_mask(mara_obj_t* parent, mara_value_t child) {
	mara_obj_t* child_obj = mara_value_to_obj(child);
	if (child_obj != NULL) {
		parent->arena_mask |= child_obj->arena_mask;
	}
}
