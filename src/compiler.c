#include "internal.h"
#include "vm.h"

MARA_PRIVATE void*
mara_barray_realloc(mara_env_t* env, void* ptr, size_t size);

#define BARRAY_REALLOC(ctx, ptr, size) mara_barray_realloc(ctx, ptr, size)
#define BARRAY_CTX_TYPE mara_env_t*
#include "barray.h"

#define MARA_MAX_NAMES UINT16_MAX
#define MARA_OP_LABEL UINT8_MAX
#define MARA_MAX_LABELS UINT16_MAX
#define MARA_MAX_FUNCTIONS UINT8_MAX
#define MARA_MAX_INSTRUCTIONS (mara_index_t)INT16_MAX

typedef struct {
	mara_instruction_t instruction;
	mara_source_info_t source_info;
} mara_tagged_instruction_t;

typedef struct mara_local_scope_s {
	struct mara_local_scope_s* parent;

	mara_value_t names;
} mara_local_scope_t;

typedef struct mara_function_scope_s {
	struct mara_function_scope_s* parent;
	mara_zone_snapshot_t zone_snapshot;

	mara_value_t args;
	mara_value_t captures;

	mara_index_t num_locals;
	mara_index_t max_num_locals;
	mara_local_scope_t* local_scope;

	mara_index_t num_temps;
	mara_index_t max_num_temps;

	mara_value_t constants;  // a map
	barray(mara_function_t*) functions;
	mara_index_t num_labels;

	barray(mara_tagged_instruction_t) instructions;
} mara_function_scope_t;

typedef struct {
	mara_exec_ctx_t* exec_ctx;
	mara_zone_t* zone;
	mara_compile_options_t options;

	mara_function_scope_t* function_scope;
	mara_debug_info_key_t debug_key;

	// Temporary list to store captures during compilation
	barray(mara_value_t) captures;

	// Core symbols
	mara_value_t sym_if;
	mara_value_t sym_def;
	mara_value_t sym_set;
	mara_value_t sym_fn;
	mara_value_t sym_do;
	mara_value_t sym_nil;
	mara_value_t sym_true;
	mara_value_t sym_false;
	mara_value_t sym_import;
	mara_value_t sym_export;
} mara_compile_ctx_t;

MARA_PRIVATE MARA_PRINTF_LIKE(3, 5) mara_error_t*
mara_compiler_error(
	mara_compile_ctx_t* ctx,
	mara_str_t type,
	const char* fmt,
	mara_value_t extra,
	...
) {
	const mara_source_info_t* debug_info = mara_get_debug_info(ctx->exec_ctx, ctx->debug_key);
	if (debug_info != NULL) { mara_set_debug_info(ctx->exec_ctx, *debug_info); }

	va_list args;
	va_start(args, extra);
	mara_error_t* error = mara_errorv(ctx->exec_ctx, type, fmt, extra, args);
	va_end(args);

	return error;
}

MARA_PRIVATE void
mara_compiler_set_debug_info(
	mara_compile_ctx_t* ctx,
	mara_obj_list_t* list,
	mara_index_t index
) {
	ctx->debug_key = mara_make_debug_info_key(
		mara_obj_to_value(mara_container_of(list, mara_obj_t, body)),
		index
	);
}

MARA_PRIVATE void*
mara_barray_realloc(mara_env_t* env, void* ptr, size_t size) {
	return mara_realloc(env->options.allocator, ptr, size);
}

MARA_PRIVATE mara_local_scope_t*
mara_compiler_begin_local_scope(mara_compile_ctx_t* ctx) {
	mara_exec_ctx_t* exec_ctx = ctx->exec_ctx;
	mara_zone_t* local_zone = mara_get_local_zone(exec_ctx);
	mara_local_scope_t* scope = MARA_ZONE_ALLOC_TYPE(
		exec_ctx, local_zone, mara_local_scope_t
	);
	*scope = (mara_local_scope_t){
		.parent = ctx->function_scope->local_scope,
		.names = mara_new_map(ctx->exec_ctx, local_zone),
	};
	ctx->function_scope->local_scope = scope;
	return scope;
}

MARA_PRIVATE void
mara_compiler_end_local_scope(mara_compile_ctx_t* ctx) {
	mara_function_scope_t* fn_scope = ctx->function_scope;
	mara_local_scope_t* local_scope = fn_scope->local_scope;

	fn_scope->max_num_locals = mara_max(fn_scope->max_num_locals, fn_scope->num_locals);
	mara_index_t num_locals_in_scope;
	mara_assert_no_error(mara_map_len(ctx->exec_ctx, local_scope->names, &num_locals_in_scope));
	fn_scope->num_locals -= num_locals_in_scope;
	fn_scope->local_scope = local_scope->parent;
}

MARA_PRIVATE mara_error_t*
mara_compiler_add_argument(mara_compile_ctx_t* ctx, mara_value_t name) {
	mara_function_scope_t* fn_scope = ctx->function_scope;

	mara_value_t existing_index;
	mara_index_t new_index;
	mara_assert_no_error(mara_map_get(ctx->exec_ctx, fn_scope->args, name, &existing_index));
	mara_assert_no_error(mara_map_len(ctx->exec_ctx, fn_scope->args, &new_index));

	if (new_index >= MARA_MAX_NAMES) {
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/limit-reached/max-arguments"),
			"Function has too many arguments",
			mara_nil()
		);
	} else if (!mara_value_is_nil(existing_index)) {
		mara_str_t name_str;
		mara_assert_no_error(mara_value_to_str(ctx->exec_ctx, name, &name_str));
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/syntax-error/duplicated-names"),
			"Argument `%.*s` is declared twice in the same argument list",
			name,
			name_str.len, name_str.data
		);
	} else {
		mara_assert_no_error(
			mara_map_set(
				ctx->exec_ctx,
				fn_scope->args,
				name, mara_value_from_int(new_index)
			)
		);
		return NULL;
	}
}

MARA_PRIVATE mara_error_t*
mara_compiler_add_capture(mara_compile_ctx_t* ctx, mara_value_t name, mara_index_t* index) {
	mara_function_scope_t* fn_scope = ctx->function_scope;

	mara_value_t existing_index;
	mara_index_t new_index;
	mara_assert_no_error(mara_map_get(ctx->exec_ctx, fn_scope->captures, name, &existing_index));
	mara_assert_no_error(mara_map_len(ctx->exec_ctx, fn_scope->captures, &new_index));
	mara_assert(mara_value_is_nil(existing_index), "Capture already exists");

	if (MARA_EXPECT(new_index < MARA_MAX_NAMES)) {
		mara_assert_no_error(
			mara_map_set(
				ctx->exec_ctx,
				fn_scope->captures,
				name, mara_value_from_int(new_index)
			)
		);
		*index = new_index;
		return NULL;
	} else {
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/limit-reached/max-captures"),
			"Function has too many captures",
			mara_nil()
		);
	}
}

MARA_PRIVATE mara_error_t*
mara_compiler_add_local(mara_compile_ctx_t* ctx, mara_value_t name, mara_index_t* index) {
	mara_function_scope_t* fn_scope = ctx->function_scope;
	mara_local_scope_t* local_scope = fn_scope->local_scope;

	mara_value_t existing_index;
	mara_assert_no_error(mara_map_get(ctx->exec_ctx, local_scope->names, name, &existing_index));

	if (!mara_value_is_nil(existing_index)) {
		mara_str_t name_str;
		mara_assert_no_error(mara_value_to_str(ctx->exec_ctx, name, &name_str));
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/syntax-error/duplicated-names"),
			"Local variable `%.*s` is declared twice in the same block",
			name,
			name_str.len, name_str.data
		);
	} else if (fn_scope->num_locals >= MARA_MAX_NAMES) {
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/limit-reached/max-locals"),
			"Function has too many local variables",
			mara_nil()
		);
	} else {
		mara_index_t new_index = *index = fn_scope->num_locals++;
		mara_assert_no_error(
			mara_map_set(
				ctx->exec_ctx,
				local_scope->names,
				name, mara_value_from_int(new_index)
			)
		);
		return NULL;
	}
}

MARA_PRIVATE mara_error_t*
mara_compiler_add_label(mara_compile_ctx_t* ctx, mara_index_t* index) {
	mara_function_scope_t* fn_scope = ctx->function_scope;
	if (MARA_EXPECT(fn_scope->num_labels < MARA_MAX_LABELS)) {
		*index = fn_scope->num_labels++;
		return NULL;
	} else {
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/limit-reached/max-labels"),
			"Function has too many labels",
			mara_nil()
		);
	}
}

MARA_PRIVATE void
mara_compiler_cleanup_function_scope(mara_env_t* env, void* userdata) {
	mara_function_scope_t* fn_scope = userdata;
	barray_free(env, fn_scope->functions);
	barray_free(env, fn_scope->instructions);
}

MARA_PRIVATE void
mara_compiler_begin_function(mara_compile_ctx_t* ctx) {
	mara_exec_ctx_t* exec_ctx = ctx->exec_ctx;
	mara_zone_t* local_zone = mara_get_local_zone(exec_ctx);
	mara_zone_snapshot_t snapshot = mara_zone_snapshot(exec_ctx);
	mara_function_scope_t* scope = MARA_ZONE_ALLOC_TYPE(
		exec_ctx, local_zone, mara_function_scope_t
	);
	*scope = (mara_function_scope_t){
		.parent = ctx->function_scope,
		.constants = mara_new_map(ctx->exec_ctx, local_zone),
		.zone_snapshot = snapshot,
		.args = mara_new_map(ctx->exec_ctx, local_zone),
		.captures = mara_new_map(ctx->exec_ctx, local_zone),
	};
	mara_add_finalizer(exec_ctx, local_zone, (mara_callback_t){
		.fn = mara_compiler_cleanup_function_scope,
		.userdata = scope
	});
	ctx->function_scope = scope;

	mara_compiler_begin_local_scope(ctx);
}

MARA_PRIVATE mara_function_t*
mara_compiler_end_function(mara_compile_ctx_t* ctx) {
	mara_compiler_end_local_scope(ctx);
	mara_function_scope_t* fn_scope = ctx->function_scope;
	mara_assert(fn_scope->local_scope == NULL, "Unbalanced local scopes");

	mara_exec_ctx_t* exec_ctx = ctx->exec_ctx;
	mara_env_t* env = exec_ctx->env;
	mara_zone_t* target_zone = ctx->zone;
	mara_zone_t* permanent_zone = &env->permanent_zone;
	mara_zone_t* local_zone = mara_get_local_zone(exec_ctx);

	// Collect label targets
	mara_index_t* jump_targets = mara_zone_alloc_ex(
		exec_ctx, local_zone,
		sizeof(mara_index_t) * fn_scope->num_labels, _Alignof(mara_index_t)
	);
	mara_index_t num_instructions = barray_len(fn_scope->instructions);
	{
		mara_index_t out_index = 0;
		for (mara_index_t i = 0; i < num_instructions; ++i) {
			mara_opcode_t opcode;
			mara_operand_t operands;
			mara_tagged_instruction_t tagged_instruction = fn_scope->instructions[i];
			mara_decode_instruction(tagged_instruction.instruction, &opcode, &operands);

			if (opcode == MARA_OP_LABEL) {
				jump_targets[operands] = out_index;
			} else {
				fn_scope->instructions[out_index++] = tagged_instruction;
			}
		}
		num_instructions = out_index;
	}

	// Rewrite jumps
	for (mara_index_t i = 0; i < num_instructions; ++i) {
		mara_opcode_t opcode;
		mara_operand_t operands;
		mara_tagged_instruction_t tagged_instruction = fn_scope->instructions[i];
		mara_decode_instruction(tagged_instruction.instruction, &opcode, &operands);

		if (opcode == MARA_OP_JUMP || opcode == MARA_OP_JUMP_IF_FALSE) {
			fn_scope->instructions[i].instruction = mara_encode_instruction(
				opcode,
				(mara_operand_t)(jump_targets[operands] - i - 1)
			);
		}
	}

	// Split tagged instructions into 2 arrays
	mara_instruction_t* instructions = mara_zone_alloc_ex(
		exec_ctx, permanent_zone,
		sizeof(mara_instruction_t) * num_instructions, _Alignof(mara_instruction_t)
	);
	mara_source_info_t* source_info = NULL;
	if (!ctx->options.strip_debug_info) {
		source_info = mara_zone_alloc_ex(
			exec_ctx, permanent_zone,
			sizeof(mara_source_info_t) * num_instructions, _Alignof(mara_source_info_t)
		);
	}
	for (mara_index_t i = 0; i < num_instructions; ++i) {
		mara_tagged_instruction_t tagged_instruction = fn_scope->instructions[i];
		instructions[i] = tagged_instruction.instruction;
		if (source_info != NULL) {
			source_info[i] = tagged_instruction.source_info;
			source_info[i].filename = mara_strpool_intern(
				exec_ctx->env, &exec_ctx->env->permanent_arena,
				&env->permanent_strpool, tagged_instruction.source_info.filename
			);
		}
	}

	// Constant pool
	mara_index_t num_constants;
	mara_value_t* constants;
	{
		mara_obj_map_t* constant_pool;
		mara_assert_no_error(mara_unbox_map(exec_ctx, fn_scope->constants, &constant_pool));

		num_constants = constant_pool->len;
		constants = mara_zone_alloc_ex(
			exec_ctx, permanent_zone,
			sizeof(mara_value_t) * num_constants, _Alignof(mara_value_t)
		);

		for (mara_obj_map_node_t* itr = constant_pool->root; itr != NULL; itr = itr->next) {
			mara_index_t constant_index;
			mara_assert_no_error(mara_value_to_int(exec_ctx, itr->value, &constant_index));
			mara_assert_no_error(mara_copy(exec_ctx, permanent_zone, itr->key, &constants[constant_index]));
		}
	}

	// Sub functions
	mara_index_t num_functions = barray_len(fn_scope->functions);
	mara_function_t** functions = mara_zone_alloc_ex(
		exec_ctx, permanent_zone,
		sizeof(mara_function_t**) * num_functions, _Alignof(mara_function_t**)
	);
	memcpy(functions, fn_scope->functions, sizeof(mara_function_t*) * num_functions);

	// Build the final function
	mara_function_t* function = MARA_ZONE_ALLOC_TYPE(
		exec_ctx, target_zone, mara_function_t
	);
	*function = (mara_function_t) {
		.stack_size = fn_scope->max_num_locals + fn_scope->max_num_temps,
		.num_instructions = num_instructions,
		.instructions = instructions,
		.source_info = source_info,
		.num_constants = num_constants,
		.constants = constants,
		.num_functions = num_functions,
		.functions = functions,
	};
	mara_assert_no_error(
		mara_map_len(exec_ctx, fn_scope->args, &function->num_args)
	);
	mara_assert_no_error(
		mara_map_len(exec_ctx, fn_scope->captures, &function->num_captures)
	);
	const mara_source_info_t* debug_info = mara_get_debug_info(
		ctx->exec_ctx, ctx->debug_key
	);
	if (debug_info != NULL) {
		function->filename = debug_info->filename;
	}

	ctx->function_scope = fn_scope->parent;
	mara_zone_restore(exec_ctx, fn_scope->zone_snapshot);

	return function;
}

MARA_PRIVATE mara_error_t*
mara_compiler_emit(
	mara_compile_ctx_t* ctx,
	mara_opcode_t opcode,
	mara_operand_t operands,
	mara_index_t temp_delta
) {
	mara_tagged_instruction_t tagged_instruction = {
		.instruction = mara_encode_instruction(opcode, operands),
	};
	const mara_source_info_t* debug_info = mara_get_debug_info(
		ctx->exec_ctx, ctx->debug_key
	);
	if (debug_info != NULL) {
		tagged_instruction.source_info = *debug_info;
	}

	mara_function_scope_t* fn_scope = ctx->function_scope;
	barray_push(ctx->exec_ctx->env, fn_scope->instructions, tagged_instruction);
	fn_scope->num_temps += temp_delta;
	fn_scope->max_num_temps = mara_max(fn_scope->max_num_temps, fn_scope->num_temps);
	if (MARA_EXPECT(barray_len(fn_scope->instructions) <= MARA_MAX_INSTRUCTIONS)) {
		return NULL;
	} else {
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/limit-reached/max-instructions"),
			"Function has too many instructions",
			mara_nil()
		);
	}
}

MARA_PRIVATE mara_error_t*
mara_compiler_find_name(
	mara_compile_ctx_t* ctx,
	mara_value_t name,
	mara_opcode_t* load_opcode_out,
	mara_index_t* index_out
) {
	mara_exec_ctx_t* exec_ctx = ctx->exec_ctx;
	mara_value_t index = mara_nil();
	bool is_new_capture = false;
	mara_opcode_t load_opcode;

	// Search upward for the variable
	for (
		mara_function_scope_t* fn_scope = ctx->function_scope;
		fn_scope != NULL;
		fn_scope = fn_scope->parent
	) {
		// First search the locals
		for (
			mara_local_scope_t* local_scope = fn_scope->local_scope;
			local_scope != NULL;
			local_scope = local_scope->parent
		) {
			mara_assert_no_error(
				mara_map_get(exec_ctx, local_scope->names, name, &index)
			);
			if (!mara_value_is_nil(index)) {
				load_opcode = MARA_OP_GET_LOCAL;
				goto end_of_search;
			}
		}

		// Then check arguments
		mara_assert_no_error(mara_map_get(exec_ctx, fn_scope->args, name, &index));
		if (!mara_value_is_nil(index)) {
			load_opcode = MARA_OP_GET_ARG;
			goto end_of_search;
		}

		// Then check captures
		mara_assert_no_error(mara_map_get(exec_ctx, fn_scope->captures, name, &index));
		if (!mara_value_is_nil(index)) {
			load_opcode = MARA_OP_GET_CAPTURE;
			goto end_of_search;
		}

		is_new_capture = true;
	}
end_of_search:
	if (mara_value_is_nil(index)) {
		mara_str_t name_str;
		mara_assert_no_error(mara_value_to_str(ctx->exec_ctx, name, &name_str));
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/name-error"),
			"Name '%.*s' is not defined",
			name,
			name_str.len, name_str.data
		);
	} else if (is_new_capture) {
		mara_check_error(mara_compiler_add_capture(ctx, name, index_out));
		*load_opcode_out = MARA_OP_GET_CAPTURE;
		return NULL;
	} else {
		mara_assert_no_error(mara_value_to_int(exec_ctx, index, index_out));
		*load_opcode_out = load_opcode;
		return NULL;
	}
}

MARA_PRIVATE mara_error_t*
mara_compile_expression(mara_compile_ctx_t* ctx, mara_value_t expr);

MARA_PRIVATE mara_error_t*
mara_compile_sequence(
	mara_compile_ctx_t* ctx,
	mara_obj_list_t* list,
	mara_index_t offset
) {
	// There is no statement, only expression.
	// Every expression must produce a result even if it's only NIL.
	// It is easier to just generate a POP before every expression.
	// The final optimization pass will delete all POP that follows NIL.
	mara_index_t list_len = list->len;
	mara_compiler_emit(ctx, MARA_OP_NIL, 0, 1);
	for (mara_index_t i = offset; i < list_len; ++i) {
		mara_compiler_set_debug_info(ctx, list, i);
		mara_compiler_emit(ctx, MARA_OP_POP, 1, -1);
		mara_check_error(mara_compile_expression(ctx, list->elems[i]));
	}

	return NULL;
}

MARA_PRIVATE mara_error_t*
mara_compile_call(mara_compile_ctx_t* ctx, mara_obj_list_t* list, mara_value_t fn) {
	mara_index_t list_len = list->len;
	for (mara_index_t i = 1; i < list_len; ++i) {
		mara_compiler_set_debug_info(ctx, list, i);
		mara_check_error(mara_compile_expression(ctx, list->elems[i]));
	}

	mara_compiler_set_debug_info(ctx, list, 0);
	mara_check_error(mara_compile_expression(ctx, fn));

	mara_compiler_set_debug_info(ctx, list, MARA_DEBUG_INFO_SELF);
	return mara_compiler_emit(ctx, MARA_OP_CALL, list_len - 1, -list_len);
}

MARA_PRIVATE mara_error_t*
mara_compile_def(mara_compile_ctx_t* ctx, mara_obj_list_t* list) {
	mara_index_t list_len = list->len;
	if (
		MARA_EXPECT(
			(list_len == 2 || list_len == 3)
			&& mara_value_is_symbol(list->elems[1])
		)
	) {
		// Compile the assignment block first so it is evaluted before the
		// variable is defined.
		if (list_len == 3) {
			mara_compiler_set_debug_info(ctx, list, 2);
			mara_check_error(mara_compile_expression(ctx, list->elems[2]));
		} else {
			mara_check_error(mara_compiler_emit(ctx, MARA_OP_NIL, 0, 1));
		}

		mara_compiler_set_debug_info(ctx, list, MARA_DEBUG_INFO_SELF);

		mara_index_t var_index;
		mara_check_error(mara_compiler_add_local(ctx, list->elems[1], &var_index));
		return mara_compiler_emit(ctx, MARA_OP_SET_LOCAL, var_index, 0);
	} else {
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/syntax-error/def"),
			"`def` must have the form `(def <name>)` or `(def <name> <value>)`"
			" where `name` is a symbol",
			mara_nil()
		);
	}
}

MARA_PRIVATE mara_error_t*
mara_compile_set(mara_compile_ctx_t* ctx, mara_obj_list_t* list) {
	mara_index_t list_len = list->len;

	if (MARA_EXPECT(list_len == 3 && mara_value_is_symbol(list->elems[1]))) {
		mara_compiler_set_debug_info(ctx, list, 1);
		mara_opcode_t load_opcode;
		mara_index_t var_index;
		mara_check_error(mara_compiler_find_name(ctx, list->elems[1], &load_opcode, &var_index));

		mara_compiler_set_debug_info(ctx, list, 2);
		mara_check_error(mara_compile_expression(ctx, list->elems[2]));

		mara_compiler_set_debug_info(ctx, list, MARA_DEBUG_INFO_SELF);
		mara_opcode_t store_opcode = MARA_OP_NOP;
		switch (load_opcode) {
			case MARA_OP_GET_LOCAL:
				store_opcode = MARA_OP_SET_LOCAL;
				break;
			case MARA_OP_GET_ARG:
				store_opcode = MARA_OP_SET_ARG;
				break;
			case MARA_OP_GET_CAPTURE:
				store_opcode = MARA_OP_SET_CAPTURE;
				break;
			default:
				mara_assert(false, "Cannot resolve store");
				break;
		}
		return mara_compiler_emit(ctx, store_opcode, var_index, 0);
	} else {
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/syntax-error/set"),
			"`set` must have the form `(set <name> <value>)` where `<name>` is a symbol",
			mara_nil()
		);
	}
}

MARA_PRIVATE mara_error_t*
mara_compile_if(mara_compile_ctx_t* ctx, mara_obj_list_t* list) {
	mara_index_t list_len = list->len;
	if (list_len == 3 || list_len == 4) {
		mara_index_t label_index;
		mara_check_error(mara_compiler_add_label(ctx, &label_index));

		// Condition
		mara_compiler_set_debug_info(ctx, list, 1);
		mara_check_error(mara_compile_expression(ctx, list->elems[1]));

		// Conditional jump over true branch
		mara_compiler_set_debug_info(ctx, list, MARA_DEBUG_INFO_SELF);
		mara_check_error(mara_compiler_emit(ctx, MARA_OP_JUMP_IF_FALSE, label_index, -1));

		// true branch
		mara_compiler_set_debug_info(ctx, list, 2);
		mara_check_error(mara_compile_expression(ctx, list->elems[2]));

		// false branch
		mara_compiler_set_debug_info(ctx, list, MARA_DEBUG_INFO_SELF);
		mara_check_error(mara_compiler_emit(ctx, MARA_OP_LABEL, label_index, 0));
		if (list_len == 4) {
			mara_compiler_set_debug_info(ctx, list, 3);
			mara_check_error(mara_compile_expression(ctx, list->elems[3]));
		} else {
			mara_check_error(mara_compiler_emit(ctx, MARA_OP_NIL, label_index, 1));
		}

		return NULL;
	} else {
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/syntax-error/if"),
			"`if` must have one of the following forms:\n"
			"\n"
			" - `(if <condition> <if-true>)`\n"
			" - `(if <condition> <if-true> <if-false>)`",
			mara_nil()
		);
	}
}

MARA_PRIVATE mara_error_t*
mara_compile_fn(mara_compile_ctx_t* ctx, mara_obj_list_t* list) {
	mara_exec_ctx_t* exec_ctx = ctx->exec_ctx;
	mara_index_t list_len = list->len;

	if (list_len < 2) { goto syntax_error; }

	mara_value_t args = list->elems[1];
	if (!mara_value_is_list(args)) { goto syntax_error; }

	mara_obj_list_t* arg_list;
	mara_assert_no_error(mara_unbox_list(ctx->exec_ctx, args, &arg_list));
	mara_index_t num_args = arg_list->len;
	for (mara_index_t i = 0; i < num_args; ++i) {
		if (!mara_value_is_symbol(arg_list->elems[i])) { goto syntax_error; }
	}

	mara_function_scope_t* fn_scope = ctx->function_scope;
	if (barray_len(fn_scope->functions) >= MARA_MAX_FUNCTIONS) {
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/limit-reached/max-functions"),
			"Function has too many subfunctions",
			mara_nil()
		);
	}

	mara_compiler_begin_function(ctx);
	for (mara_index_t i = 0; i < num_args; ++i) {
		mara_compiler_set_debug_info(ctx, arg_list, i);
		mara_check_error(mara_compiler_add_argument(ctx, arg_list->elems[i]));
	}

	mara_compiler_set_debug_info(ctx, list, MARA_DEBUG_INFO_SELF);
	mara_check_error(mara_compile_sequence(ctx, list, 2));

	mara_compiler_set_debug_info(ctx, list, MARA_DEBUG_INFO_SELF);

	// Copy the captures to a temporary list
	mara_obj_map_t* captures;
	mara_assert_no_error(mara_unbox_map(exec_ctx, ctx->function_scope->captures, &captures));
	mara_index_t num_captures = captures->len;
	barray_resize(ctx->exec_ctx->env, ctx->captures, num_captures);
	for (
		mara_obj_map_node_t* itr = captures->root;
		itr != NULL;
		itr = itr->next
	) {
		mara_index_t index;
		mara_assert_no_error(mara_value_to_int(exec_ctx, itr->value, &index));
		ctx->captures[index] = itr->key;
	}

	// Finish the sub function and emit closure
	mara_function_t* subfunction = mara_compiler_end_function(ctx);
	mara_index_t function_index = barray_len(fn_scope->functions);
	barray_push(ctx->exec_ctx->env, fn_scope->functions, subfunction);
	mara_operand_t operand = ((function_index & 0xff) << 16) | (num_captures & 0xffff);
	mara_check_error(mara_compiler_emit(ctx, MARA_OP_MAKE_CLOSURE, operand, 1));
	// All num_captures instructions following this are pseudo-instruction.
	// They are not actually executed and only consulted by the VM when creating
	// the closure.
	// This is much faster since we don't have to dispatch them from the dispatch
	// loop.
	for (mara_index_t i = 0; i < num_captures; ++i) {
		mara_opcode_t load_opcode = MARA_OP_NIL;
		mara_index_t var_index;
		// Captures will be added to parent too
		mara_assert_no_error(
			mara_compiler_find_name(ctx, ctx->captures[i], &load_opcode, &var_index)
		);
		// The pseudo instruction is not executed so there is no impact on stack size
		mara_check_error(mara_compiler_emit(ctx, load_opcode, var_index, 0));
	}

	return NULL;

syntax_error:
	return mara_compiler_error(
		ctx,
		mara_str_from_literal("core/syntax-error/fn"),
		"`fn` must have the form `(fn (<arguments>) <body>)` where `arguments` are symbols",
		mara_nil()
	);
}

MARA_PRIVATE mara_error_t*
mara_compile_do(mara_compile_ctx_t* ctx, mara_obj_list_t* list) {
	mara_compiler_begin_local_scope(ctx);
	mara_error_t* error = mara_compile_sequence(ctx, list, 1);
	mara_compiler_end_local_scope(ctx);
	return error;
}

MARA_PRIVATE mara_error_t*
mara_compile_list(mara_compile_ctx_t* ctx, mara_value_t expr) {
	mara_obj_list_t* list;
	mara_assert_no_error(mara_unbox_list(ctx->exec_ctx, expr, &list));

	ctx->debug_key = mara_make_debug_info_key(expr, MARA_DEBUG_INFO_SELF);

	mara_index_t list_len = list->len;
	if (MARA_EXPECT(list_len > 0)) {
		mara_value_t first_elem = list->elems[0];

		if (mara_value_is_symbol(first_elem)) {
			if (first_elem == ctx->sym_def) {
				return mara_compile_def(ctx, list);
			} else if (first_elem == ctx->sym_set) {
				return mara_compile_set(ctx, list);
			} else if (first_elem == ctx->sym_if) {
				return mara_compile_if(ctx, list);
			} else if (first_elem == ctx->sym_fn) {
				return mara_compile_fn(ctx, list);
			} else if (first_elem == ctx->sym_do) {
				return mara_compile_do(ctx, list);
			} else if (
				first_elem == ctx->sym_nil
				|| first_elem == ctx->sym_true
				|| first_elem == ctx->sym_false
			) {
				ctx->debug_key.index = 0;
				return mara_compiler_error(
					ctx,
					mara_str_from_literal("core/unexpected-type"),
					"This symbol is not callable",
					first_elem
				);
			} else {
				return mara_compile_call(ctx, list, first_elem);
			}
		} else if (mara_value_is_list(first_elem)) {
			return mara_compile_call(ctx, list, first_elem);
		} else {
			ctx->debug_key.index = 0;
			return mara_compiler_error(
				ctx,
				mara_str_from_literal("core/unexpected-type"),
				"First element of a call must be a list or a symbol, not %s",
				mara_nil(),
				mara_value_type_name(mara_value_type(first_elem, NULL))
			);
		}
	} else {
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/syntax-error/empty-list"),
			"Empty list cannot be compiled",
			mara_nil()
		);
	}
}

MARA_PRIVATE mara_error_t*
mara_compile_expression(mara_compile_ctx_t* ctx, mara_value_t expr) {
	mara_exec_ctx_t* exec_ctx = ctx->exec_ctx;

	if (mara_value_is_nil(expr)) {
		return mara_compiler_emit(ctx, MARA_OP_NIL, 0, 1);
	} else if (mara_value_is_true(expr)) {
		return mara_compiler_emit(ctx, MARA_OP_TRUE, 0, 1);
	} else if (mara_value_is_false(expr)) {
		return mara_compiler_emit(ctx, MARA_OP_FALSE, 0, 1);
	} else if (
		mara_value_is_str(expr)
		// TODO: use MARA_OP_SMALL_INT
		|| mara_value_is_int(expr)
		|| mara_value_is_real(expr)
	) {
		mara_index_t constant_index;
		{
			mara_value_t constant_pool = ctx->function_scope->constants;

			mara_value_t lookup_result;
			mara_map_get(exec_ctx, constant_pool, expr, &lookup_result);
			if (mara_value_is_int(lookup_result)) {
				mara_assert_no_error(
					mara_value_to_int(exec_ctx, lookup_result, &constant_index)
				);
			} else {
				mara_map_len(exec_ctx, constant_pool, &constant_index);
				mara_map_set(
					exec_ctx, constant_pool, expr, mara_value_from_int(constant_index)
				);
			}
		}

		return mara_compiler_emit(ctx, MARA_OP_CONSTANT, constant_index, 1);
	} else if (mara_value_is_symbol(expr)) {
		mara_opcode_t load_opcode;
		mara_index_t var_index;
		mara_check_error(mara_compiler_find_name(ctx, expr, &load_opcode, &var_index));
		return mara_compiler_emit(ctx, load_opcode, var_index, 1);
	} else if (mara_value_is_list(expr)) {
		return mara_compile_list(ctx, expr);
	} else {
		return mara_compiler_error(
			ctx,
			mara_str_from_literal("core/unexpected-type"),
			"Expression of type %s cannot be compiled",
			mara_nil(),
			mara_value_type_name(mara_value_type(expr, NULL))
		);
	}
}

MARA_PRIVATE mara_error_t*
mara_do_compile(
	mara_compile_ctx_t* ctx,
	mara_zone_t* zone,
	mara_compile_options_t options,
	mara_value_t exprs,
	mara_value_t* result
) {
	mara_compiler_begin_function(ctx);
	if (options.as_module) {
		mara_check_error(mara_compiler_add_argument(ctx, ctx->sym_import));
		mara_check_error(mara_compiler_add_argument(ctx, ctx->sym_export));
	}

	mara_obj_list_t* list;
	mara_check_error(mara_unbox_list(ctx->exec_ctx, exprs, &list));
	mara_compiler_set_debug_info(ctx, list, MARA_DEBUG_INFO_SELF);
	mara_check_error(mara_compile_sequence(ctx, list, 0));

	mara_compiler_set_debug_info(ctx, list, MARA_DEBUG_INFO_SELF);
	mara_check_error(mara_compiler_emit(ctx, MARA_OP_RETURN, 0, 0));

	mara_function_t* function = mara_compiler_end_function(ctx);

	mara_obj_t* obj = mara_alloc_obj(ctx->exec_ctx, zone, sizeof(mara_obj_closure_t));
	obj->type = MARA_OBJ_TYPE_MARA_FN;
	mara_obj_closure_t* closure = (mara_obj_closure_t*)obj->body;
	closure->fn = function;

	*result = mara_obj_to_value(obj);
	return NULL;
}

mara_error_t*
mara_compile(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_compile_options_t options,
	mara_value_t exprs,
	mara_value_t* result
) {
	mara_error_t* error;
	mara_zone_enter_new(ctx, (mara_zone_options_t){
		.num_marked_zones = 1,
		.marked_zones = (mara_zone_t*[]){ zone },
	});

	mara_compile_ctx_t compile_ctx = {
		.exec_ctx = ctx,
		.zone = zone,
		.options = options,

		// Sync this list with symtab.c
		.sym_def = mara_new_symbol(ctx, mara_str_from_literal("def")),
		.sym_set = mara_new_symbol(ctx, mara_str_from_literal("set")),
		.sym_if = mara_new_symbol(ctx, mara_str_from_literal("if")),
		.sym_fn = mara_new_symbol(ctx, mara_str_from_literal("fn")),
		.sym_do = mara_new_symbol(ctx, mara_str_from_literal("do")),
		.sym_nil = mara_new_symbol(ctx, mara_str_from_literal("nil")),
		.sym_true = mara_new_symbol(ctx, mara_str_from_literal("true")),
		.sym_false = mara_new_symbol(ctx, mara_str_from_literal("false")),
		.sym_import = mara_new_symbol(ctx, mara_str_from_literal("import")),
		.sym_export = mara_new_symbol(ctx, mara_str_from_literal("export")),
	};

	error = mara_do_compile(&compile_ctx, zone, options, exprs, result);

	barray_free(ctx->env, compile_ctx.captures);
	mara_zone_exit(ctx);
	return error;
}
