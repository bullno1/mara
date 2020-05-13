#include "helpers.h"
#include <bk/default_allocator.h>
#include <stdio.h>


static void
panic_handler(
	mara_context_t* ctx,
	const char* message,
	const char* file,
	unsigned int line
)
{
	fprintf(stderr, "%s:%d:%p: %s\n", file, line, (void*)ctx, message);
	abort();
}


mara_context_config_t
mara_default_context_config()
{
	return (mara_context_config_t) {
		.allocator = bk_default_allocator,
		.panic_handler = panic_handler,
		.main_thread_config = {
			.stack_size = 2048
		}
	};
}
