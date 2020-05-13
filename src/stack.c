#include "internal.h"


MARA_DECL mara_index_t
mara_stack_len(mara_context_t* ctx)
{
	return ctx->current_thread->
}

MARA_DECL void
mara_push_null(mara_context_t* ctx);

MARA_DECL void
mara_push_number(mara_context_t* ctx, mara_number_t num);

MARA_DECL void
mara_push_string(mara_context_t* ctx, mara_string_ref_t str);

MARA_DECL void
mara_push_stringfv(mara_context_t* ctx, const char* fmt, va_list arg);

MARA_DECL void
mara_make_symbol(mara_context_t* ctx, mara_index_t index);

MARA_DECL void
mara_dup(mara_context_t* ctx, mara_index_t index);
