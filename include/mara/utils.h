#ifndef MARA_UTILS_H
#define MARA_UTILS_H

#include <mara.h>
#include <string.h>

#define mara_str_from_literal(str) \
	(mara_str_t){ .data = str, .len = sizeof(str) - 1 }

static inline mara_str_t
mara_str_from_cstr(const char* cstr) {
	return (mara_str_t){
		.len = strlen(cstr),
		.data = cstr,
	};
}

#endif
