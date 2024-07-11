#include "internal.h"
#include <mara/bind.h>
#include "vm_intrinsics.h"

// List

MARA_PRIVATE MARA_FUNCTION(mara_core_list_new) {
	(void)userdata;
	mara_add_native_debug_info(ctx);

	mara_index_t capacity = 0;
	if (argc >= 1) {
		MARA_FN_BIND_ARG(capacity, 0);
	}
	MARA_RETURN(mara_new_list(ctx, mara_get_return_zone(ctx), capacity));
}

MARA_PRIVATE MARA_FUNCTION(mara_core_list_len) {
	(void)userdata;
	mara_add_native_debug_info(ctx);
	MARA_FN_CHECK_ARITY(1);
	MARA_FN_ARG(mara_list_t*, list, 0);

	MARA_RETURN(mara_list_len(ctx, list));
}

MARA_PRIVATE MARA_FUNCTION(mara_core_list_push) {
	(void)userdata;
	mara_add_native_debug_info(ctx);
	MARA_FN_CHECK_ARITY(2);
	MARA_FN_ARG(mara_list_t*, list, 0);
	MARA_FN_ARG(mara_value_t, value, 1);

	mara_list_push(ctx, list, value);
	MARA_RETURN(mara_nil());
}

MARA_PRIVATE MARA_FUNCTION(mara_core_list_set) {
	(void)userdata;
	mara_add_native_debug_info(ctx);
	MARA_FN_CHECK_ARITY(3);
	MARA_FN_ARG(mara_list_t*, list, 0);
	MARA_FN_ARG(mara_index_t, index, 1);
	MARA_FN_ARG(mara_value_t, value, 2);

	MARA_RETURN(mara_list_set(ctx, list, index, value));
}

MARA_PRIVATE MARA_FUNCTION(mara_core_list_get) {
	(void)userdata;
	mara_add_native_debug_info(ctx);
	MARA_FN_CHECK_ARITY(2);
	MARA_FN_ARG(mara_list_t*, list, 0);
	MARA_FN_ARG(mara_index_t, index, 1);

	MARA_RETURN(mara_list_get(ctx, list, index));
}

MARA_PRIVATE MARA_FUNCTION(mara_core_module_entry) {
	(void)argv;
	(void)userdata;
	MARA_FN_CHECK_ARITY(3);

	MARA_EXPORT_FN(<, mara_intrin_lt, mara_nil());
	MARA_EXPORT_FN(<=, mara_intrin_lte, mara_nil());
	MARA_EXPORT_FN(>, mara_intrin_gt, mara_nil());
	MARA_EXPORT_FN(>=, mara_intrin_gte, mara_nil());

	MARA_EXPORT_FN(+, mara_intrin_plus, mara_nil());
	MARA_EXPORT_FN(-, mara_intrin_minus, mara_nil());

	MARA_EXPORT_FN(list/new, mara_core_list_new, mara_nil());
	MARA_EXPORT_FN(list/len, mara_core_list_len, mara_nil());
	MARA_EXPORT_FN(list/push, mara_core_list_push, mara_nil());
	MARA_EXPORT_FN(list/set, mara_core_list_set, mara_nil());
	MARA_EXPORT_FN(list/get, mara_core_list_get, mara_nil());

	MARA_RETURN(mara_value_from_bool(true));
}

MARA_PRIVATE MARA_FUNCTION(mara_load_core_module) {
	(void)ctx;
	MARA_FN_CHECK_ARITY(1);
	MARA_FN_ARG(mara_value_t, module_name, 0);

	if (module_name.internal != userdata.internal) {
		*result = mara_nil();
		return NULL;
	}

	MARA_RETURN(mara_new_fn(ctx, mara_get_return_zone(ctx), mara_core_module_entry, mara_nil()));
}

void
mara_add_core_module(mara_exec_ctx_t* ctx) {
	mara_value_t module_name = mara_new_sym(ctx, mara_str_from_literal("core"));
	mara_fn_t* loader = mara_new_fn(
		ctx, ctx->current_zone,
		mara_load_core_module,
		module_name
	);
	mara_add_module_loader(ctx, loader);
	mara_value_t result;
	mara_import(ctx, mara_str_from_literal("core"), mara_str_from_literal("*main*"), &result);
}
