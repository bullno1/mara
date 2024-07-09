#include "internal.h"
#include <mara/bind.h>
#include "vm_intrinsics.h"

#define MARA_EXPORT_INTRINSIC(NAME, OP) \
	do { \
		mara_fn_t* fn = mara_new_fn_ex( \
			ctx, \
			mara_get_local_zone(ctx), \
			mara_intrin_##OP, \
			(mara_native_fn_options_t){ \
				.no_alloc = true \
			}, \
			MARA_OP_##OP \
		); \
		mara_export( \
			ctx, \
			mara_str_from_literal(MARA_BIND_STRINGIFY(NAME)), \
			mara_value_from_fn(fn) \
		); \
	} while (0)

// List

MARA_PRIVATE MARA_FUNCTION(mara_core_module_entry) {
	(void)argv;
	(void)userdata;
	MARA_FN_CHECK_ARITY(3);

	MARA_EXPORT_INTRINSIC(<, LT);
	MARA_EXPORT_INTRINSIC(<=, LTE);
	MARA_EXPORT_INTRINSIC(>, GT);
	MARA_EXPORT_INTRINSIC(>=, GTE);
	MARA_EXPORT_INTRINSIC(+, PLUS);
	MARA_EXPORT_INTRINSIC(-, MINUS);

	MARA_EXPORT_INTRINSIC(list, MAKE_LIST);
	MARA_EXPORT_INTRINSIC(list/new, LIST_NEW);
	MARA_EXPORT_INTRINSIC(list/get, LIST_GET);
	MARA_EXPORT_INTRINSIC(list/set, LIST_SET);
	MARA_EXPORT_INTRINSIC(list/len, LIST_LEN);
	MARA_EXPORT_INTRINSIC(list/push, LIST_PUSH);

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

	mara_native_fn_options_t entry_options = { 0 };
	MARA_RETURN(mara_new_fn(ctx, mara_get_return_zone(ctx), mara_core_module_entry, entry_options));
}

void
mara_add_core_module(mara_exec_ctx_t* ctx) {
	mara_value_t userdata = mara_new_sym(ctx, mara_str_from_literal("core"));
	mara_fn_t* loader = mara_new_fn(
		ctx, ctx->current_zone,
		mara_load_core_module,
		(mara_native_fn_options_t) {
			.userdata = &userdata,
		}
	);
	mara_add_module_loader(ctx, loader);
	mara_value_t result;
	mara_import(ctx, mara_str_from_literal("core"), mara_str_from_literal("*main*"), &result);
}
