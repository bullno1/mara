#include <munit/munit.h>
#include <mara.h>
#include <bk/default_allocator.h>
#include <bk/fs/mem.h>
#include "helpers.h"
#include "../src/internal.h"
#include "../src/lexer.h"


typedef struct fixture_s
{
	mara_context_t* ctx;
	mara_lexer_t lexer;
} fixture_t;


static void*
setup(const MunitParameter params[], void* user_data) {
	fixture_t* fixture = BK_NEW(bk_default_allocator, fixture_t);
	mara_context_config_t config = mara_default_context_config();
	fixture->ctx = mara_create_context(&config);
	mara_lexer_init(fixture->ctx, &fixture->lexer);

	return fixture;
}

static void
tear_down(void* fixturep) {
	fixture_t* fixture = fixturep;
	mara_lexer_cleanup(&fixture->lexer);
	mara_destroy_context(fixture->ctx);
	bk_free(bk_default_allocator, fixture);
}

static void
mara_assert_token_equal(mara_token_t lhs, mara_token_t rhs)
{
	mara_assert_enum(mara_token_type_t, lhs.type, ==, rhs.type);
	mara_assert_source_range_equal(lhs.location, rhs.location);
	mara_assert_string_ref_equal(lhs.lexeme, rhs.lexeme);
}

static MunitResult
normal(const MunitParameter params[], void* fixture)
{
	(void)params;
	mara_lexer_t* lexer = &((fixture_t*)fixture)->lexer;

	char text[] = (
		"(\n"
		"  )\n"
		"50.6 \"hi hi \" \r\n"
		"; comment 12 \"hi\" \r\n"
		"\n"
		"test-23 -3 -ve -\r\n"
		"\n"
		" \"f \\\"\"\n"
		"  \" \\\\\" 3_3.3"
	);
	bk_mem_file_t mem_file;
	bk_file_t* file = bk_mem_fs_wrap_fixed(
		&mem_file,
		text, BK_STATIC_ARRAY_LEN(text) - 1 // excluding the ending NULL
	);
	mara_lexer_reset(lexer, file);

	mara_token_t expected_tokens[] = {
		{
			.type = MARA_TOKEN_LPAREN,
			.lexeme = mara_string_ref("("),
			.location = {
				.start = {.line = 1, .column = 1},
				.end = {.line = 1, .column = 1}
			}
		},
		{
			.type = MARA_TOKEN_RPAREN,
			.lexeme = mara_string_ref(")"),
			.location = {
				.start = {.line = 2, .column = 3},
				.end = {.line = 2, .column = 3},
			}
		},
		{
			.type = MARA_TOKEN_NUMBER,
			.lexeme = mara_string_ref("50.6"),
			.location = {
				.start = {.line = 3, .column = 1},
				.end = {.line = 3, .column = 4}
			}
		},
		{
			.type = MARA_TOKEN_STRING,
			.lexeme = mara_string_ref("hi hi "),
			.location = {
				.start = {.line = 3, .column = 6},
				.end = {.line = 3, .column = 13}
			}
		},
		{
			.type = MARA_TOKEN_SYMBOL,
			.lexeme = mara_string_ref("test-23"),
			.location = {
				.start = {.line = 6, .column = 1},
				.end = {.line = 6, .column = 7}
			}
		},
		{
			.type = MARA_TOKEN_NUMBER,
			.lexeme = mara_string_ref("-3"),
			.location = {
				.start = {.line = 6, .column = 9},
				.end = {.line = 6, .column = 10}
			}
		},
		{
			.type = MARA_TOKEN_SYMBOL,
			.lexeme = mara_string_ref("-ve"),
			.location = {
				.start = {.line = 6, .column = 12},
				.end = {.line = 6, .column = 14}
			}
		},
		{
			.type = MARA_TOKEN_SYMBOL,
			.lexeme = mara_string_ref("-"),
			.location = {
				.start = {.line = 6, .column = 16},
				.end = {.line = 6, .column = 16}
			}
		},
		{
			.type = MARA_TOKEN_STRING,
			.lexeme = mara_string_ref("f \\\""),
			.location = {
				.start = {.line = 8, .column = 2},
				.end = {.line = 8, .column = 7}
			}
		},
		{
			.type = MARA_TOKEN_STRING,
			.lexeme = mara_string_ref(" \\\\"),
			.location = {
				.start = {.line = 9, .column = 3},
				.end = {.line = 9, .column = 7}
			}
		},
		{
			.type = MARA_TOKEN_NUMBER,
			.lexeme = mara_string_ref("33.3"),
			.location = {
				.start = {.line = 9, .column = 9},
				.end = {.line = 9, .column = 13}
			}
		},
	};

	mara_token_t token;
	size_t num_tokens = BK_STATIC_ARRAY_LEN(expected_tokens);
	for(size_t i = 0; i < num_tokens; ++i)
	{
		mara_token_t* expected_token = &expected_tokens[i];
		mara_lexer_status_t status = mara_lexer_next_token(lexer, &token);

		munit_logf(
			MUNIT_LOG_INFO, "%s %s '%.*s' (%u:%u - %u:%u)",
			mara_lexer_status_t_to_str(status) + sizeof("MARA"),
			mara_token_type_t_to_str(token.type) + sizeof("MARA"),
			(int)token.lexeme.length,
			token.lexeme.ptr,
			token.location.start.line,
			token.location.start.column,
			token.location.end.line,
			token.location.end.column
		);

		mara_assert_enum(mara_lexer_status_t, MARA_LEXER_OK, ==, status);
		mara_assert_token_equal(*expected_token, token);
	}

	mara_lexer_status_t end_status = mara_lexer_next_token(lexer, &token);
	munit_logf(MUNIT_LOG_INFO, "%.*s", (int)token.lexeme.length, token.lexeme.ptr);

	mara_assert_enum(
		mara_lexer_status_t,
		MARA_LEXER_END, ==, end_status
	);

	return MUNIT_OK;
}

static void
mara_assert_bad_string(
	mara_lexer_t* lexer,
	char* string,
	mara_source_addr_t start_line,
	mara_source_addr_t start_col,
	mara_source_addr_t end_line,
	mara_source_addr_t end_col
)
{
	bk_mem_file_t file;
	bk_file_t* input = bk_mem_fs_wrap_fixed(
		&file, string, strlen(string)
	);
	mara_lexer_reset(lexer, input);
	mara_token_t token;
	mara_assert_enum(
		mara_lexer_status_t,
		MARA_LEXER_BAD_STRING, ==, mara_lexer_next_token(lexer, &token)
	);
	mara_source_range_t source_range = {
		.start = {
			.line = start_line,
			.column = start_col,
		},
		.end = {
			.line = end_line,
			.column = end_col,
		},
	};
	mara_assert_source_range_equal(source_range, token.location);
}

static MunitResult
bad_string(const MunitParameter params[], void* fixture)
{
	(void)params;
	mara_lexer_t* lexer = &((fixture_t*)fixture)->lexer;

	mara_assert_bad_string(lexer, " \"ha", 1, 2, 1, 4);
	mara_assert_bad_string(lexer, " \" \n\"", 1, 2, 1, 3);
	mara_assert_bad_string(lexer, " \"  \r\"", 1, 2, 1, 4);

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/normal",
		.test = normal,
		.setup = setup,
		.tear_down = tear_down
	},
	{
		.name = "/bad_string",
		.test = bad_string,
		.setup = setup,
		.tear_down = tear_down
	},
	{ 0 }
};

MunitSuite lexer = {
	.prefix = "/lexer",
	.tests = tests
};
