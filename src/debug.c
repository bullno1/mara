#include "internal.h"

void
mara_set_debug_info(mara_exec_ctx_t* ctx, mara_source_info_t debug_info) {
	ctx->current_zone->debug_info = debug_info;
}

mara_source_info_t
mara_get_debug_info(mara_exec_ctx_t* ctx, mara_value_t value);
