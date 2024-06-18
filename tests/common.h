#ifndef MARA_TESTS_COMMON_H
#define MARA_TESTS_COMMON_H

#define MARA_ASSERT_NO_ERROR(CALL) \
	do { \
		mara_error_t* error = CALL; \
		ASSERT_TRUE_INFO( \
			error == NULL, \
			"%.*s: %.*s", \
			error->type.len, error->type.data, \
			error->message.len, error->message.data \
		); \
	} while (0)

#endif
