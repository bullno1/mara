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

typedef struct {
	subcommand_t* command;
	int argc;
	const char** argv;
	int exit_code;
} subcommand_args_t;

int
parse(int argc, const char* argv[], mara_exec_ctx_t* ctx);

static const char *const usages[] = {
    "mara [common-options] parse [command-options] <args>",
	NULL
};

static inline void
subcommand_thunk(mara_exec_ctx_t* ctx, void* userdata) {
	subcommand_args_t* args = userdata;
	args->exit_code = args->command->fn(args->argc, args->argv, ctx);
}

int
main(int argc, const char* argv[]) {
	subcommand_t subcommands[] = {
		{ "parse", parse },
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
	subcommand_args_t subcommand_args = {
		.argc = argc,
		.argv = argv,
		.exit_code = 1,
	};
    for (size_t i = 0; i < sizeof(subcommands) / sizeof(subcommands[0]); i++) {
        if (strcmp(subcommands[i].name, command_name) == 0) {
			subcommand_args.command = &subcommands[i];
			break;
        }
    }

	if (subcommand_args.command != NULL) {
		mara_env_t* env = mara_create_env((mara_env_options_t) { 0 });
		mara_exec(env, (mara_callback_t){
			.fn = subcommand_thunk,
			.userdata = &subcommand_args,
		});
		mara_destroy_env(env);
		return subcommand_args.exit_code;
	} else {
		fprintf(stderr, "Invalid command: %s\n", command_name);
		argparse_usage(&argparse);
		return 1;
	}
}
