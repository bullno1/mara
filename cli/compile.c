#include <mara.h>
#include <mara/utils.h>
#include <stdio.h>
#include <errno.h>
#include "vendor/argparse/argparse.h"

int
compile(int argc, const char* argv[], mara_exec_ctx_t* ctx) {
	const char* const usage[] = {
		"compile [options] [--] [filename]",
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
		"Compile a file and pretty print the result.",
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

	mara_value_t expr;
	mara_error_t* error = mara_parse_all(
		ctx,
		mara_get_local_zone(ctx),
		mara_str_from_cstr(filename),
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

	mara_value_t fn;
	error = mara_compile(
		ctx,
		mara_get_local_zone(ctx),
		(mara_compile_options_t){
			.as_module = true,
		},
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

	mara_print_value(
		ctx,
		fn,
		(mara_print_options_t){ 0 },
		(mara_writer_t){
			.fn = mara_write_to_file,
			.userdata = stdout,
		}
	);

end:
	if (input != stdin && input != NULL) {
		fclose(input);
	}

	return exit_code;
}
