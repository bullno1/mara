#include "internal.h"
#include "vendor/nanbox.h"
#include "nanoprintf.h"

_Static_assert(sizeof(nanbox_t) == sizeof(mara_value_t), "mara_value_t cannot be nan-boxed");

MARA_PRIVATE mara_value_t
mara_nanbox_to_value(nanbox_t nanbox) {
	return (mara_value_t){ .internal = nanbox.as_int64 };
}

MARA_PRIVATE nanbox_t
mara_value_to_nanbox(mara_value_t value) {
	return (nanbox_t){ .as_int64 = value.internal };
}

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
	// zone->arena_mask is guaranteed to be set since we just alloc'ed
	obj->arena_mask = zone->arena_mask;

	return obj;
}

bool
mara_value_is_obj(mara_value_t value) {
	return nanbox_is_pointer(mara_value_to_nanbox(value));
}

mara_obj_t*
mara_value_to_obj(mara_value_t value) {
	return nanbox_to_pointer(mara_value_to_nanbox(value));
}

mara_value_t
mara_obj_to_value(mara_obj_t* obj) {
	return mara_nanbox_to_value(nanbox_from_pointer(obj));
}

bool
mara_value_is_nil(mara_value_t value) {
	nanbox_t nanbox = mara_value_to_nanbox(value);
	return nanbox_is_null(nanbox);
}

bool
mara_value_is_int(mara_value_t value) {
	nanbox_t nanbox = mara_value_to_nanbox(value);
	return nanbox_is_int(nanbox);
}

bool
mara_value_is_real(mara_value_t value) {
	nanbox_t nanbox = mara_value_to_nanbox(value);
	return nanbox_is_double(nanbox);
}

bool
mara_value_is_bool(mara_value_t value) {
	nanbox_t nanbox = mara_value_to_nanbox(value);
	return nanbox_is_boolean(nanbox);
}

bool
mara_value_is_str(mara_value_t value) {
	if (mara_value_is_obj(value)) {
		mara_obj_t* obj = mara_value_to_obj(value);
		return obj->type == MARA_OBJ_TYPE_STRING;
	} else {
		return false;
	}
}

bool
mara_value_is_sym(mara_value_t value) {
	nanbox_t nanbox = mara_value_to_nanbox(value);
	return nanbox_is_aux(nanbox) && nanbox.as_bits.tag == NANBOX_MIN_AUX_TAG;
}

bool
mara_value_is_ref(mara_value_t value, void* tag) {
	if (mara_value_is_obj(value)) {
		mara_obj_t* obj = mara_value_to_obj(value);
		return obj->type == MARA_OBJ_TYPE_REF
			&& ((mara_ref_t*)obj->body)->tag == tag;
	} else {
		return false;
	}
}

bool
mara_value_is_fn(mara_value_t value) {
	if (mara_value_is_obj(value)) {
		mara_obj_t* obj = mara_value_to_obj(value);
		return obj->type == MARA_OBJ_TYPE_VM_CLOSURE
			|| obj->type == MARA_OBJ_TYPE_NATIVE_CLOSURE;
	} else {
		return false;
	}
}

bool
mara_value_is_list(mara_value_t value) {
	if (mara_value_is_obj(value)) {
		mara_obj_t* obj = mara_value_to_obj(value);
		return obj->type == MARA_OBJ_TYPE_LIST;
	} else {
		return false;
	}
}

bool
mara_value_is_map(mara_value_t value) {
	if (mara_value_is_obj(value)) {
		mara_obj_t* obj = mara_value_to_obj(value);
		return obj->type == MARA_OBJ_TYPE_MAP;
	} else {
		return false;
	}
}

mara_value_type_t
mara_value_type(mara_value_t value, void** tag) {
	nanbox_t nanbox = mara_value_to_nanbox(value);
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
		return MARA_VAL_SYM;
	} else if (nanbox_is_pointer(nanbox)) {
		mara_obj_t* obj = nanbox_to_pointer(nanbox);
		switch (obj->type) {
			case MARA_OBJ_TYPE_STRING:
				return MARA_VAL_STR;
			case MARA_OBJ_TYPE_REF:
				if (tag != NULL) {
					*tag = ((mara_ref_t*)obj->body)->tag;
				}
				return MARA_VAL_REF;
			case MARA_OBJ_TYPE_VM_CLOSURE:
			case MARA_OBJ_TYPE_NATIVE_CLOSURE:
				return MARA_VAL_FN;
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
mara_value_to_int(mara_exec_ctx_t* ctx, mara_value_t value, mara_index_t* result) {
	nanbox_t nanbox = mara_value_to_nanbox(value);
	if (MARA_EXPECT(nanbox_is_int(nanbox))) {
		*result = nanbox_to_int(nanbox);
		return NULL;
	} else if (nanbox_is_double(nanbox)) {
		*result = (mara_index_t)nanbox_to_double(nanbox);
		return NULL;
	} else {
		return mara_type_error(ctx, MARA_VAL_INT, value);
	}
}

mara_error_t*
mara_value_to_real(mara_exec_ctx_t* ctx, mara_value_t value, mara_real_t* result) {
	nanbox_t nanbox = mara_value_to_nanbox(value);
	if (MARA_EXPECT(nanbox_is_double(nanbox))) {
		*result = nanbox_to_double(nanbox);
		return NULL;
	} else if (nanbox_is_int(nanbox)) {
		*result = (mara_real_t)nanbox_to_int(nanbox);
		return NULL;
	} else {
		return mara_type_error(ctx, MARA_VAL_REAL, value);
	}
}

mara_error_t*
mara_value_to_bool(mara_exec_ctx_t* ctx, mara_value_t value, bool* result) {
	nanbox_t nanbox = mara_value_to_nanbox(value);
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
	} else if (mara_value_is_sym(value)) {
		nanbox_t nanbox = mara_value_to_nanbox(value);
		*result = mara_symtab_lookup(&ctx->env->symtab, nanbox.as_bits.payload);
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
		*result = ((mara_ref_t*)obj->body)->value;
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

mara_error_t*
mara_value_to_list(mara_exec_ctx_t* ctx, mara_value_t value, mara_list_t** result) {
	if (MARA_EXPECT(mara_value_is_list(value))) {
		mara_obj_t* obj = mara_value_to_obj(value);
		*result = (mara_list_t*)obj->body;
		return NULL;
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/unexpected-type"),
			"Expecting %s got %s",
			mara_nil(),
			"list",
			mara_value_type_name(mara_value_type(value, NULL))
		);
	}
}

mara_error_t*
mara_value_to_map(mara_exec_ctx_t* ctx, mara_value_t value, mara_map_t** result) {
	if (MARA_EXPECT(mara_value_is_map(value))) {
		mara_obj_t* obj = mara_value_to_obj(value);
		*result = (mara_map_t*)obj->body;
		return NULL;
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/unexpected-type"),
			"Expecting %s got %s",
			mara_nil(),
			"map",
			mara_value_type_name(mara_value_type(value, NULL))
		);
	}
}

mara_error_t*
mara_value_to_fn(mara_exec_ctx_t* ctx, mara_value_t value, mara_fn_t** result) {
	if (MARA_EXPECT(mara_value_is_fn(value))) {
		mara_obj_t* obj = mara_value_to_obj(value);
		*result = (mara_fn_t*)obj;
		return NULL;
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/unexpected-type"),
			"Expecting %s got %s",
			mara_nil(),
			"function",
			mara_value_type_name(mara_value_type(value, NULL))
		);
	}
}

mara_value_t
mara_nil(void) {
	return mara_nanbox_to_value(nanbox_null());
}

mara_value_t
mara_value_from_bool(bool value) {
	return mara_nanbox_to_value(nanbox_from_boolean(value));
}

mara_value_t
mara_value_from_int(mara_index_t value) {
	return mara_nanbox_to_value(nanbox_from_int(value));
}

mara_value_t
mara_value_from_real(mara_real_t value) {
	return mara_nanbox_to_value(nanbox_from_double(value));
}

mara_value_t
mara_value_from_list(mara_list_t* list) {
	return list != NULL
		? mara_obj_to_value(mara_container_of(list, mara_obj_t, body))
		: mara_nil();
}

mara_value_t
mara_value_from_map(mara_map_t* map) {
	return map != NULL
		? mara_obj_to_value(mara_container_of(map, mara_obj_t, body))
		: mara_nil();
}

mara_value_t
mara_value_from_fn(mara_fn_t* fn) {
	return fn != NULL
		? mara_obj_to_value((mara_obj_t*)fn)
		: mara_nil();
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
	char* chars = (char*)str + sizeof(mara_str_t);
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
	mara_obj_t* obj = mara_alloc_obj(ctx, zone, sizeof(mara_ref_t));
	obj->type = MARA_OBJ_TYPE_REF;

	mara_ref_t* ref = (mara_ref_t*)obj->body;
	ref->tag = tag;
	ref->value = value;

	return mara_obj_to_value(obj);
}

mara_fn_t*
mara_new_fn(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_native_fn_t fn,
	mara_native_fn_options_t options
) {
	// TODO: Implement light native function like Lua
	// if userdata == NULL, use a light representation.
	// Intern the function pointer in the permanent zone.
	mara_obj_t* obj = mara_alloc_obj(ctx, zone, sizeof(mara_native_closure_t));
	obj->type = MARA_OBJ_TYPE_NATIVE_CLOSURE;

	mara_native_closure_t* closure = (mara_native_closure_t*)obj->body;
	closure->fn = fn;
	closure->options = options;

	// Point to the header so we can differentiate between mara and native closures
	return (mara_fn_t*)obj;
}

mara_value_t
mara_tombstone(void) {
	return mara_nanbox_to_value(nanbox_deleted());
}

bool
mara_value_is_tombstone(mara_value_t value) {
	return nanbox_is_deleted(mara_value_to_nanbox(value));
}

bool
mara_value_is_true(mara_value_t value) {
	return nanbox_is_true(mara_value_to_nanbox(value));
}

bool
mara_value_is_false(mara_value_t value) {
	return nanbox_is_false(mara_value_to_nanbox(value));
}

mara_value_t
mara_new_sym(mara_exec_ctx_t* ctx, mara_str_t name) {
	mara_env_t* env = ctx->env;
	mara_index_t id = mara_symtab_intern(ctx->env, &env->symtab, name);

	nanbox_t nanbox = {
		.as_bits = { .payload = id, .tag = NANBOX_MIN_AUX_TAG },
	};
	return mara_nanbox_to_value(nanbox);
}

void
mara_obj_add_arena_mask(mara_obj_t* parent, mara_value_t child) {
	if (mara_value_is_obj(child)) {
		mara_obj_t* child_obj = mara_value_to_obj(child);
		parent->arena_mask |= child_obj->arena_mask;
	}
}
