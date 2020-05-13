#ifndef MARA_VALUE_NAN_BOX_H
#define MARA_VALUE_NAN_BOX_H

union mara_value_u
{
	double d64;
	uint64_t u64;
};

typedef union mara_value_u mara_value_t;

#endif
