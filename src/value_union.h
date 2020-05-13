#ifndef MARA_VALUE_UNION_H
#define MARA_VALUE_UNION_H

struct mara_value_s
{
	mara_value_type_t type;

	union
	{
		bool boolean;
		mara_number_t number;
		void* ptr;
	} data;
};

typedef struct mara_value_s mara_value_t;

#endif
