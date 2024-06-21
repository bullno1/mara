#include "internal.h"
#include "mara.h"

MARA_PRIVATE mara_error_t*
mara_internal_import(
	mara_exec_ctx_t* ctx,
	mara_index_t argc,
	const mara_value_t* argv,
	void* userdata,
	mara_value_t* result
) {
	MARA_ADD_NATIVE_DEBUG_INFO(ctx);

	(void)userdata;

	if (argc != 2) {
		// TODO: collect standard errors into a file: mara/errors.h
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/wrong-arity"),
			"Function expects 2 arguments, got %d",
			mara_nil(),
			argc
		);
	}

	mara_str_t module_name;
	mara_check_error(mara_value_to_str(ctx, argv[0], &module_name));
	// Qualify relative import
	if (
		module_name.len > 2
		&& module_name.data[0] == '.'
		&& module_name.data[1] == '/'
	) {
		mara_str_t calling_module = ctx->current_module_options.module_name;
		mara_value_t qualified_name = mara_new_strf(
			ctx, ctx->current_zone, "%.*s/%.*s",
			calling_module.len, calling_module.data,
			module_name.len - 2, module_name.data + 2
		);
		mara_assert_no_error(
			mara_value_to_str(ctx, qualified_name, &module_name)
		);
	}
	mara_value_t module_name_sym = mara_new_symbol(ctx, module_name);

	mara_str_t export_name;
	mara_check_error(mara_value_to_str(ctx, argv[1], &export_name));
	mara_value_t export_name_sym = mara_new_symbol(ctx, export_name);

	mara_value_t existing_module;
	mara_assert_no_error(
		mara_map_get(
			ctx, ctx->env->module_cache,
			module_name_sym, &existing_module
		)
	);

	if (MARA_EXPECT(mara_value_is_map(existing_module))) {
		mara_value_t export;
		mara_assert_no_error(
			mara_map_get(ctx, existing_module, export_name_sym, &export)
		);

		if (MARA_EXPECT(!mara_value_is_nil(export))) {
			*result = export;
			return NULL;
		} else {
			return mara_errorf(
				ctx,
				mara_str_from_literal("core/name-error"),
				"Name '%.*s' is not defined in module '%.*s'",
				mara_nil(),
				export_name.len, export_name.data,
				module_name.len, module_name.data
			);
		}
	} else if (mara_value_is_nil(existing_module)) {
		mara_value_t module_entry = mara_nil();
		if (
			!mara_value_is_nil(ctx->current_module)
			&& mara_value_is_list(ctx->module_loaders)
		) {
			mara_obj_list_t* loaders;
			mara_assert_no_error(
				mara_unbox_list(ctx, ctx->module_loaders, &loaders)
			);
			mara_value_t calling_module = mara_new_symbol(
				ctx, ctx->current_module_options.module_name
			);
			mara_index_t num_loaders = loaders->len;
			for (mara_index_t i = 0; i < num_loaders; ++i) {
				// TODO: use sub zones??
				mara_value_t args[] = { module_name_sym, calling_module };
				mara_error_t* load_error = mara_call(
					ctx, ctx->current_zone,
					loaders->elems[i],
					sizeof(args) / sizeof(args[0]), args,
					&module_entry
				);
				// TODO: where to output warning?
				if (
					load_error == NULL
					&& mara_value_is_function(module_entry)
				) {
					break;
				}
			}
		}

		if (MARA_EXPECT(mara_value_is_function(module_entry))) {
			mara_check_error(
				mara_init_module(ctx, module_entry, (mara_module_options_t){
					.module_name = module_name,
				})
			);

			mara_assert_no_error(
				mara_map_get(
					ctx, ctx->env->module_cache,
					module_name_sym, &existing_module
				)
			);

			mara_value_t export;
			mara_assert_no_error(
				mara_map_get(ctx, existing_module, export_name_sym, &export)
			);

			if (MARA_EXPECT(!mara_value_is_nil(export))) {
				*result = export;
				return NULL;
			} else {
				return mara_errorf(
					ctx,
					mara_str_from_literal("core/name-error"),
					"Name '%.*s' is not defined in module '%.*s'",
					mara_nil(),
					export_name.len, export_name.data,
					module_name.len, module_name.data
				);
			}
		} else {
			return mara_errorf(
				ctx,
				mara_str_from_literal("core/module-not-found"),
				"Could not load module '%.*s'",
				mara_nil(),
				module_name.len, module_name.data
			);
		}
	} else if (mara_value_is_false(existing_module)) {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/circular-dependency"),
			"Module %.*s is being loaded",
			mara_nil(),
			module_name.len, module_name.data
		);
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/panic"),
			"Module cache contains invalid value",
			existing_module
		);
	}
}

MARA_PRIVATE mara_error_t*
mara_internal_export(
	mara_exec_ctx_t* ctx,
	mara_index_t argc,
	const mara_value_t* argv,
	void* userdata,
	mara_value_t* result
) {
	(void)userdata;
	MARA_ADD_NATIVE_DEBUG_INFO(ctx);

	if (argc != 2) {
		// TODO: collect standard errors into a file: mara/errors.h
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/wrong-arity"),
			"Function expects 2 arguments, got %d",
			mara_nil(),
			argc
		);
	}

	mara_str_t export_name;
	mara_check_error(mara_value_to_str(ctx, argv[0], &export_name));
	mara_value_t export_name_sym = mara_new_symbol(ctx, export_name);

	if (
		mara_value_is_map(ctx->current_module)
		&& !ctx->current_module_options.ignore_export
	) {
		mara_assert_no_error(
			mara_map_set(ctx, ctx->env->module_cache, export_name_sym, argv[1])
		);
	}

	*result = argv[1];
	return NULL;
}

MARA_PRIVATE void
mara_clear_module_loaders(mara_env_t* env, void* userdata) {
	(void)env;
	mara_exec_ctx_t* ctx = userdata;
	ctx->module_loaders = mara_nil();
}

mara_error_t*
mara_init_module(
	mara_exec_ctx_t* ctx,
	mara_value_t entry_fn,
	mara_module_options_t options
) {
	if (MARA_EXPECT(mara_value_is_function(entry_fn))) {
		// Avoid allocation in the permanent zone until the module is confirmed
		mara_zone_t* local_zone = ctx->current_zone;

		mara_env_t* env = ctx->env;
		if (mara_value_is_nil(env->module_cache)) {
			env->module_cache = mara_new_map(ctx, &env->permanent_zone);
		}

		mara_value_t module_name = mara_new_symbol(ctx, options.module_name);
		mara_value_t existing_module;
		mara_assert_no_error(mara_map_get(ctx, env->module_cache, module_name, &existing_module));
		if (MARA_EXPECT(mara_value_is_nil(existing_module))) {
			mara_value_t module = mara_new_map(ctx, local_zone);
			mara_value_t previous_module = ctx->current_module;
			mara_module_options_t previous_module_options = ctx->current_module_options;

			// Mark the module as being loaded
			mara_assert_no_error(
				mara_map_set(
					ctx, env->module_cache,
					module_name, mara_value_from_bool(false)
				)
			);

			// Userdata cannot be safely used as module code may save these functions
			mara_value_t import_fn = mara_new_fn(ctx, local_zone, (mara_native_fn_t){
				.fn = mara_internal_import,
			});
			mara_value_t export_fn = mara_new_fn(ctx, local_zone, (mara_native_fn_t){
				.fn = mara_internal_export,
			});
			mara_value_t args[] = { import_fn, export_fn };
			ctx->current_module = module;
			ctx->current_module_options = options;
			mara_value_t result;
			mara_error_t* error = mara_call(
				ctx, local_zone,
				entry_fn, sizeof(args) / sizeof(args[0]), args,
				&result
			);
			ctx->current_module = previous_module;
			ctx->current_module_options = previous_module_options;

			if (error == NULL) {
				mara_assert_no_error(
					mara_map_set(
						ctx, module,
						// TODO: maybe cache this?
						mara_new_symbol(ctx, mara_str_from_literal("*main*")),
						result
					)
				);
				mara_assert_no_error(
					mara_map_set(ctx, env->module_cache, module_name, module);
				);
			} else {
				mara_assert_no_error(
					mara_map_set(ctx, env->module_cache, module_name, mara_nil());
				);
			}

			return error;
		} else {
			bool being_loaded = mara_value_is_false(existing_module);
			return mara_errorf(
				ctx,
				mara_str_from_literal("core/duplicated-module"),
				"Module %.*s is %s",
				mara_nil(),
				options.module_name.len, options.module_name.data,
				being_loaded ? "being loaded" : "already loaded"
			);
		}
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/unexpected-type"),
			"Expecting %s got %s",
			mara_nil(),
			"function",
			mara_value_type_name(mara_value_type(entry_fn, NULL))
		);
	}
}

mara_error_t*
mara_add_module_loader(mara_exec_ctx_t* ctx, mara_value_t fn) {
	if (MARA_EXPECT(mara_value_is_function(fn))) {
		if (!mara_value_is_list(ctx->module_loaders)) {
			ctx->module_loaders = mara_new_list(ctx, ctx->current_zone, 1);
			// This list is created in this zone and implicitly passed to all
			// subsequent zones.
			// It cannot exist after this zone is exited.
			mara_add_finalizer(ctx, ctx->current_zone, (mara_callback_t){
				.fn = mara_clear_module_loaders,
				.userdata = ctx,
			});
		}

		return mara_list_push(ctx, ctx->module_loaders, fn);
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/unexpected-type"),
			"Expecting %s got %s",
			mara_nil(),
			"function",
			mara_value_type_name(mara_value_type(fn, NULL))
		);
	}
}

void
mara_add_standard_loader(mara_exec_ctx_t* ctx, mara_module_fs_t fs);
