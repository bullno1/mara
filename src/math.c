#include <math.h>
#include <mara.h>


#define MARA_TYPE_CHECK(CTX, INDEX, TYPE) \
	do { \
		if(!MARA_TYPE_CHECK_FUNC(TYPE)(CTX, INDEX)) { \
			MARA_ERROR(CTX, "Bad argument #%d (expecting %s)", INDEX, #TYPE); \
		} \
	} while(0)

#define MARA_ERROR(CTX, FORMAT, ...) \
	do { \
		mara_push_stringf(CTX, FORMAT, __VA_ARGS__); \
		return MARA_EXEC_ERROR; \
	} while(0)

#define MARA_RETURN(CTX, TYPE, VALUE) \
	do { \
		MARA_PUSH_FUNC(TYPE)(CTX, VALUE); \
		return MARA_EXEC_OK; \
	} while(0)

#define MARA_BIND_NUMBER(CTX, INDEX, VAR) \
	do { \
		MARA_TYPE_CHECK(CTX, INDEX, number); \
		VAR = mara_as_number(CTX, INDEX); \
	} while(0)

#define MARA_BIND_STRING(CTX, INDEX, VAR) \
	do { \
		MARA_TYPE_CHECK(CTX, INDEX, string); \
		VAR = mara_as_string(CTX, INDEX); \
	} while(0)

#define MARA_TYPE_CHECK_FUNC(TYPE) MARA_PP_CONCAT(mara_is_, TYPE)
#define MARA_PUSH_FUNC(TYPE) MARA_PP_CONCAT(mara_push_, TYPE)

#define MARA_PP_CONCAT(A, B) MARA_PP_CONCAT2(A, B)
#define MARA_PP_CONCAT2(A, B) MARA_PP_CONCAT3(A, B)
#define MARA_PP_CONCAT3(A, B) A##B
#define MARA_BIND_DECLARE_STRUCT(NAME, X) \
	struct NAME { X(MARA_BIND_DECLARE_STRUCT_FIELD) }
#define MARA_BIND_DECLARE_STRUCT_FIELD(NATIVE_TYPE, MARA_TYPE, NAME) \
	NATIVE_TYPE NAME;
#define MARA_DEFINE_RECORD_PUSH(NATIVE_TYPE, MARA_TYPE, FIELD) \
	MARA_PP_CONCAT(mara_push_, MARA_TYPE)(ctx, value.FIELD);
#define MARA_DEFINE_RECORD_DECL(MARA_TYPE)

#ifdef MARA_BIND_IMPL
#define MARA_BIND_DECLARE_STRUCT_PUSH(NATIVE_TYPE, MARA_TYPE, X) \
	void MARA_PP_CONCAT(mara_push_, MARA_TYPE)(mara_context_t* ctx, NATIVE_TYPE value);
#else
#define MARA_BIND_DECLARE_STRUCT_PUSH(NATIVE_TYPE, MARA_TYPE, X) \
	void MARA_PP_CONCAT(mara_push_, MARA_TYPE)(mara_context_t* ctx, NATIVE_TYPE value) { \
		X(MARA_DEFINE_RECORD_PUSH) \
	}
#endif

#define MARA_SOURCE_RANGE(X) \
	X(uint16_t, number, line) \
	X(uint16_t, number, column) \

MARA_BIND_DECLARE_STRUCT(mara_source_range, MARA_SOURCE_RANGE);
MARA_BIND_DECLARE_STRUCT_PUSH(struct mara_source_range, source_range, MARA_SOURCE_RANGE)


mara_exec_t
mara_math_sin(mara_context_t* ctx)
{
	mara_number_t number;

	MARA_TYPE_CHECK(ctx, 0, number);
	MARA_RETURN(ctx, number, sin(number));
}
