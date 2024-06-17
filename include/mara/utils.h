#ifndef MARA_UTILS_H
#define MARA_UTILS_H

#include <mara.h>
#include <string.h>

#define mara_max(a, b) ((a) > (b) ? (a) : (b))
#define mara_min(a, b) ((a) < (b) ? (a) : (b))

#define mara_str_from_literal(str) \
	(mara_str_t){ .data = str, .len = sizeof(str) - 1 }

#define mara_check_error(op) \
	do { \
		mara_error_t* error = (op); \
		if (error != NULL) { return error; } \
	} while (0)

#define MARA_NATIVE_DEBUG_INFO(ctx) \
	do { \
		mara_set_debug_info(ctx, (mara_source_info_t){ \
			.filename = mara_str_from_literal(__FILE__), \
			.range = { \
				.start = { .line = __LINE__, .col = 1, .byte_offset = -1 }, \
				.end = { .line = __LINE__, .col = 0, .byte_offset = -1 }, \
			} \
		}); \
	} while (0)

#define mara_container_of(ptr, type, member) \
	(void*)((char*)ptr - offsetof(type, member))

static inline mara_str_t
mara_str_from_cstr(const char* cstr) {
	return (mara_str_t){
		.len = strlen(cstr),
		.data = cstr,
	};
}

static inline bool
mara_str_equal(mara_str_t lhs, mara_str_t rhs) {
	return memcmp(lhs.data, rhs.data, mara_min(lhs.len, rhs.len)) == 0;
}

static inline mara_index_t
mara_read(void* buffer, mara_index_t size, mara_reader_t reader) {
	return reader.fn(buffer, size, reader.userdata);
}

static inline mara_index_t
mara_write(const void* buffer, mara_index_t size, mara_writer_t writer) {
	return writer.fn(buffer, size, writer.userdata);
}

#endif
