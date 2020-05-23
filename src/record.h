#ifndef MARA_RECORD_H
#define MARA_RECORD_H

#include "internal.h"


mara_record_info_t*
mara_create_record_info(
	mara_context_t* ctx,
	mara_string_t* name,
	int num_attrs,
	mara_string_t* attributes[]
);


#endif
