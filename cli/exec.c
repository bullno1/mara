#include <mara.h>
#include <mara/utils.h>
#include <stdio.h>
#include <errno.h>
#include "vendor/argparse/argparse.h"

int
exec(int argc, const char* argv[], mara_exec_ctx_t* ctx) {
	const char* const usage[] = {
		"exec [options] [--] [filename]",
		NULL,
	};
	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_END(),
	};
	struct argparse argparse;
    argparse_init(&argparse, options, usage, 0);
	argparse_describe(
		&argparse,
		"Compile a file and execute it, printing the result.",
		"By default, read from stdin.\n"
		"If filename is `-` it is also treated as stdin."
	);
	argc = argparse_parse(&argparse, argc, argv);
	if (argc > 1 || argparse.state != ARGPARSE_OK) {
		argparse_usage(&argparse);
		return argparse.state != ARGPARSE_NEED_HELP;
	}

	FILE* input = stdin;
	const char* filename = "<stdin>";
	int exit_code = 0;
	if (argc == 1 && strcmp(argv[0], "-")) {
		filename = argv[0];
		errno = 0;
		input = fopen(filename, "rb");
		if (input == NULL) {
			fprintf(stderr, "Could not open %s: %s\n", filename, strerror(errno));
			exit_code = 1;
			goto end;
		}
	}

	mara_list_t* expr;
	mara_error_t* error = mara_parse(
		ctx,
		mara_get_local_zone(ctx),
		(mara_parse_options_t) {
			.filename = mara_str_from_cstr(filename)
		},
		(mara_reader_t){
			.fn = mara_read_from_file,
			.userdata = input,
		},
		&expr
	);

	if (error != NULL) {
		mara_print_error(
			ctx,
			error,
			(mara_print_options_t){ 0 },
			(mara_writer_t){
				.fn = mara_write_to_file,
				.userdata = stderr,
			}
		);

		exit_code = 1;
		goto end;
	}

	mara_fn_t* fn;
	error = mara_compile(
		ctx,
		mara_get_local_zone(ctx),
		(mara_compile_options_t){ 0 },
		expr,
		&fn
	);

	if (error != NULL) {
		mara_print_error(
			ctx,
			error,
			(mara_print_options_t){ 0 },
			(mara_writer_t){
				.fn = mara_write_to_file,
				.userdata = stderr,
			}
		);

		exit_code = 1;
		goto end;
	}

	mara_add_native_debug_info(ctx);
	mara_add_core_module(ctx);
	error = mara_init_module(
		ctx,
		(mara_module_options_t){
			.ignore_export = true,
			.module_name = mara_str_from_literal("*main*"),
		},
		fn
	);

	if (error != NULL) {
		mara_print_error(
			ctx,
			error,
			(mara_print_options_t){ 0 },
			(mara_writer_t){
				.fn = mara_write_to_file,
				.userdata = stderr,
			}
		);

		exit_code = 1;
		goto end;
	}

end:
	if (input != stdin && input != NULL) {
		fclose(input);
	}

	return exit_code;
}
