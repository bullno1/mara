#ifndef MARA_TRACKER_H
#define MARA_TRACKER_H

#include "internal.h"


typedef struct mara_tracker_s mara_tracker_t;
typedef struct mara_obj_loc_s mara_obj_loc_t;


struct mara_obj_loc_t
{
	mara_string_t* string;
	mara_source_range_t location;
};


void
mara_tracker_init(mara_ctx_t* ctx, mara_tracker_t* tracker);

void
mara_tracker_cleanup(mara_tracker_t* tracker);

void
mara_tracker_add(
	mara_tracker_t* tracker,
	void* obj,
	mara_obj_loc_t location
);

void
mara_tracker_remove(mara_tracker_t* tracker, void* obj);

mara_obj_loc_t
mara_tracker_find(mara_tracker_t* tracker, void* obj);

#endif
