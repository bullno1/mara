#include <mara.h>
#include <mara/utils.h>
#include <stdio.h>
#include <errno.h>
#include "vendor/argparse/argparse.h"

int
parse(int argc, const char* argv[], mara_exec_ctx_t* ctx) {
	const char* const usage[] = {
		"parse [options] [--] [filename]",
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
		"Parse a file and pretty print the expressions.",
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

	mara_value_t result;
	mara_error_t* error = mara_parse_all(
		ctx,
		mara_get_local_zone(ctx),
		mara_str_from_cstr(filename),
		(mara_reader_t){
			.fn = mara_read_from_file,
			.userdata = input,
		},
		&result
	);

	if (error != NULL) {
		fprintf(
			stderr,
			"%.*s: %.*s\n",
			error->type.len, error->type.data,
			error->message.len, error->message.data
		);

		exit_code = 1;
		goto end;
	}

end:
	if (input != stdin) {
		fclose(input);
	}

	return exit_code;
}
