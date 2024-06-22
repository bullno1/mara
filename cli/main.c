#include <mara.h>
#include <mara/utils.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "vendor/argparse/argparse.h"

#define ARGPARSE_CHECKED_PARSE(argparse, argc, argv) \
	do { \
		argc = argparse_parse(&argparse, argc, argv); \
		if (argc < 0) { \
			argparse_usage(&argparse); \
			return 1; \
		} \
	} while (0)

typedef int (*subcommand_fn_t)(int argc, const char* argv[], mara_exec_ctx_t* ctx);

typedef struct {
	const char* name;
	subcommand_fn_t fn;
} subcommand_t;

int
parse(int argc, const char* argv[], mara_exec_ctx_t* ctx);

int
compile(int argc, const char* argv[], mara_exec_ctx_t* ctx);

int
exec(int argc, const char* argv[], mara_exec_ctx_t* ctx);

static const char *const usages[] = {
    "mara [common-options] parse [command-options] <args>",
    "mara [common-options] compile [command-options] <args>",
    "mara [common-options] exec [command-options] <args>",
	NULL
};

int
main(int argc, const char* argv[]) {
	subcommand_t subcommands[] = {
		{ "parse", parse },
		{ "compile", compile },
		{ "exec", exec },
	};

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_END(),
	};
	struct argparse argparse;
    argparse_init(&argparse, options, usages, ARGPARSE_STOP_AT_NON_OPTION);
    argparse_describe(
		&argparse,
		"Common options:",
		"\nUse --help in each subcommand to find out more."
	);
	argc = argparse_parse(&argparse, argc, argv);
	if (argc < 1 || argparse.state != ARGPARSE_OK) {
		argparse_usage(&argparse);
		return argparse.state != ARGPARSE_NEED_HELP;
	}

	const char* command_name = argv[0];
	subcommand_fn_t command = NULL;
    for (size_t i = 0; i < sizeof(subcommands) / sizeof(subcommands[0]); i++) {
        if (strcmp(subcommands[i].name, command_name) == 0) {
			command = subcommands[i].fn;
			break;
        }
    }

	if (command != NULL) {
		mara_env_t* env = mara_create_env((mara_env_options_t) { 0 });
		mara_exec_ctx_t* ctx = mara_begin(env);
		int exit_code = command(argc, argv, ctx);
		mara_end(ctx);
		mara_destroy_env(env);
		return exit_code;
	} else {
		fprintf(stderr, "Invalid command: %s\n", command_name);
		argparse_usage(&argparse);
		return 1;
	}
}
