#include "internal.h"

MARA_PRIVATE mara_error_t*
mara_internal_import(
	mara_exec_ctx_t* ctx,
	mara_index_t argc,
	const mara_value_t* argv,
	void* userdata,
	mara_value_t* result
) {
	mara_add_native_debug_info(ctx);

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

	mara_str_t export_name;
	mara_check_error(mara_value_to_str(ctx, argv[1], &export_name));
	return mara_import(ctx, module_name, export_name, result);
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
	mara_add_native_debug_info(ctx);

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
	mara_export(ctx, export_name, argv[1]);
	*result = argv[1];
	return NULL;
}

MARA_PRIVATE void
mara_clear_module_loaders(mara_env_t* env, void* userdata) {
	(void)env;
	mara_exec_ctx_t* ctx = userdata;
	ctx->module_loaders = NULL;
}

MARA_PRIVATE mara_error_t*
mara_internal_init_module(
	mara_exec_ctx_t* ctx,
	mara_module_options_t options,
	mara_fn_t* entry_fn,
	mara_map_t** result
) {
	// Avoid allocation in the permanent zone until the module is confirmed
	mara_zone_t* local_zone = ctx->current_zone;

	mara_env_t* env = ctx->env;
	if (env->module_cache == NULL) {
		env->module_cache = mara_new_map(ctx, &env->permanent_zone);
	}

	mara_value_t module_name = mara_nil();
	mara_value_t existing_module = mara_nil();
	if (!options.ignore_export) {
		module_name = mara_new_sym(ctx, options.module_name);
		existing_module = mara_map_get(ctx, env->module_cache, module_name);
	}

	if (MARA_EXPECT(mara_value_is_nil(existing_module))) {
		mara_map_t* module = mara_new_map(ctx, local_zone);
		mara_map_t* previous_module = ctx->current_module;
		mara_module_options_t previous_module_options = ctx->current_module_options;

		// Mark the module as being loaded
		if (!options.ignore_export) {
			mara_map_set(ctx, env->module_cache, module_name, mara_value_from_bool(false));
		}

		// Userdata cannot be safely used as module code may save these functions
		mara_fn_t* import_fn = mara_new_fn(ctx, local_zone, mara_internal_import, NULL);
		mara_fn_t* export_fn = mara_new_fn(ctx, local_zone, mara_internal_export, NULL);
		mara_value_t args[] = {
			mara_value_from_fn(import_fn),
			mara_value_from_fn(export_fn),
			module_name
		};
		ctx->current_module = module;
		ctx->current_module_options = options;
		mara_value_t entry_result;
		mara_error_t* error = mara_call(
			ctx, local_zone,
			entry_fn, sizeof(args) / sizeof(args[0]), args,
			&entry_result
		);
		ctx->current_module = previous_module;
		ctx->current_module_options = previous_module_options;

		if (error == NULL) {
			// TODO: maybe cache the symbol?
			if (!options.ignore_export) {
				mara_map_set(ctx, module, mara_new_sym(ctx, mara_str_from_literal("*main*")), entry_result);
				mara_map_set(ctx, env->module_cache, module_name, mara_value_from_map(module));
			}
			*result = module;
		} else {
			mara_map_set(ctx, env->module_cache, module_name, mara_nil());
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
}

mara_error_t*
mara_init_module(
	mara_exec_ctx_t* ctx,
	mara_module_options_t options,
	mara_fn_t* entry_fn
) {
	mara_map_t* result;
	return mara_internal_init_module(ctx, options, entry_fn, &result);
}

void
mara_add_module_loader(mara_exec_ctx_t* ctx, mara_fn_t* fn) {
	if (ctx->module_loaders == NULL) {
		ctx->module_loaders = mara_new_list(ctx, ctx->current_zone, 1);
		// This list is created in this zone and implicitly passed to all
		// subsequent zones.
		// It cannot exist after this zone is exited.
		mara_add_finalizer(ctx, ctx->current_zone, (mara_callback_t){
			.fn = mara_clear_module_loaders,
			.userdata = ctx,
		});
	}

	mara_list_push(ctx, ctx->module_loaders, mara_value_from_fn(fn));
}

mara_error_t*
mara_import(
	mara_exec_ctx_t* ctx,
	mara_str_t module_name,
	mara_str_t export_name,
	mara_value_t* result
) {
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
	mara_value_t module_name_sym = mara_new_sym(ctx, module_name);
	mara_value_t export_name_sym = mara_new_sym(ctx, export_name);

	mara_value_t existing_module = mara_map_get(ctx, ctx->env->module_cache, module_name_sym);

	if (MARA_EXPECT(mara_value_is_map(existing_module))) {
		mara_map_t* module_map;
		mara_assert_no_error(mara_value_to_map(ctx, existing_module, &module_map));
		mara_value_t export = mara_map_get(ctx, module_map, export_name_sym);

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
		mara_fn_t* module_entry = NULL;
		mara_list_t* loaders = ctx->module_loaders;
		if (loaders != NULL) {
			mara_value_t calling_module = mara_new_sym(
				ctx, ctx->current_module_options.module_name
			);
			mara_index_t num_loaders = loaders->len;
			for (mara_index_t i = 0; i < num_loaders; ++i) {
				if (!mara_value_is_fn(loaders->elems[i])) { continue; }

				mara_fn_t* loader = NULL;
				mara_assert_no_error(
					mara_value_to_fn(ctx, loaders->elems[i], &loader)
				);
				if (MARA_EXPECT(loader != NULL)) {
					mara_value_t result;
					mara_value_t args[] = { module_name_sym, calling_module };
					mara_error_t* load_error = mara_call(
						ctx, ctx->current_zone,
						loader, mara_count_of(args), args,
						&result
					);
					// TODO: where to output warning?
					if (load_error == NULL && mara_value_is_fn(result)) {
						mara_assert_no_error(
							mara_value_to_fn(ctx, result, &module_entry)
						);
						break;
					}
				}
			}
		}

		if (MARA_EXPECT(module_entry != NULL)) {
			mara_module_options_t module_options = {
				.module_name = module_name,
			};
			mara_map_t* module;
			mara_check_error(
				mara_internal_init_module(ctx, module_options, module_entry, &module)
			);

			mara_value_t export = mara_map_get(ctx, module, export_name_sym);

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

bool
mara_export(mara_exec_ctx_t* ctx, mara_str_t export_name, mara_value_t value) {
	mara_value_t export_name_sym = mara_new_sym(ctx, export_name);

	if (
		MARA_EXPECT(
			ctx->current_module != NULL
			&& !ctx->current_module_options.ignore_export
		)
	) {
		mara_map_set(ctx, ctx->env->module_cache, export_name_sym, value);
		return true;
	} else {
		return false;
	}
}
