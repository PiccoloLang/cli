#include <stdio.h>
#include <string.h>

#define PICCOLO_ENABLE_DEBUG_LIB

#include "piccolo/include.h"
#include "piccolo/stdlib/stdlib.h"
#include "piccolo/debug/disassembler.h"

void printError(const char* format, va_list args) {
    vfprintf(stderr, format, args);
}

typedef enum {
    CLI_ARG_SHORT,
    CLI_ARG_LONG,
    CLI_ARG_RAW
} cli_argType;

typedef void (*cli_argHandler)(const cli_argType, const size_t, const char* const restrict, void* const restrict);

void cli_parseArgs(const int argc, const char* const restrict* restrict const argv, const cli_argHandler handler, const size_t n_short_args, const char* restrict short_args, const size_t n_long_args, const char* const restrict* const restrict long_args, void* const restrict passthrough) {
	if(!argc) return;
	if(!argv) return;
	if(argc < 0) return;
	if(!handler) return;
	if(n_short_args && !short_args) return;
	if(n_long_args && !long_args) return;

	// Precalculating the long args' lengths to save time while looping
	// We need this weirdness to avoid creating a zero-length VLA
	size_t long_arg_lens[n_long_args ? n_long_args : 1];
	if(n_long_args) {
		for(register size_t i = 0; i < n_long_args; ++i) {
			long_arg_lens[i] = 0;
			while(long_args[i][long_arg_lens[i]]) long_arg_lens[i]++;
		}
	}

	for(register size_t i = 0; i < argc - 1; i++) {
        const char const* arg = (argv + 1)[i];

		cli_argType type;
		size_t argn = SIZE_MAX;
		const char* value = NULL;

		if(arg[0] == '-') {
			if(arg[1] == '-' && n_long_args) {
				type = CLI_ARG_LONG;
				for(register size_t j = 0; j < n_long_args; ++j) {
					const size_t arg_len = long_arg_lens[j];

					// Calculating inline like this might be very slightly faster
					// given the specific use-case
					for(register size_t k = 0; k < arg_len; ++k) {
						if(long_args[j][k] != arg[k + 2]) goto long_arg_continue;
					}

					argn = j;
					if(arg[arg_len + 2]) value = arg + 2 + arg_len + 1;

				long_arg_continue:
					continue;
				}
			}
			else if(n_short_args) {
				type = CLI_ARG_SHORT;
				for(register size_t j = 0; j < n_short_args; ++j) {
					if(short_args[j] != arg[1]) continue;

					argn = j;

					if(arg[2]) value = arg + 2;
					break;
				}
			}
			else {
				type = CLI_ARG_RAW;
				argn = 0;
				value = arg;
			}
		}
		else {
			type = CLI_ARG_RAW;
			argn = 0;
			value = arg;
		}

		handler(type, argn, value, passthrough);
	}

    return;
}

struct cli_argInfo {
    const char* package;
    bool debug;
    const char* path;
};

// -f -b style args
static char short_args[] = { 'd' };
// --fizz --buzz style args
static char* long_args[] = { "debug", "pkg-path" };

static void handler(const cli_argType type, const size_t n_arg, const char* const restrict arg, void* const restrict passthrough) {
    struct cli_argInfo* const arg_info = passthrough;

    switch(type) {
        case CLI_ARG_SHORT: {
            if(n_arg == 0) arg_info->debug = true;
            break;
        }
        case CLI_ARG_LONG: {
            if(n_arg == 0) arg_info->debug = true;
            else if(n_arg == 1) arg_info->path = arg;
            break;
        }
        case CLI_ARG_RAW: {
            arg_info->package = arg;
            break;
        }
    }
}

int main(int argc, const char** argv) {
    struct cli_argInfo arg_info = { 0 };
    cli_parseArgs(argc, argv, handler, sizeof(short_args) / sizeof(short_args[0]), short_args, sizeof(long_args) / sizeof(long_args[0]), long_args, &arg_info);

    if(arg_info.debug) {
        printf("Package = %s\n", arg_info.package);
        printf("Debug = %s\n", arg_info.debug ? "true" : "false");
        printf("Search Path = %s\n", arg_info.path);
        
    }

    if(!arg_info.package) {
        fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
        return 1;
    }

    struct piccolo_Engine engine;
    piccolo_initEngine(&engine, printError);
    piccolo_addIOLib(&engine);
    piccolo_addTimeLib(&engine);
    if(arg_info.debug) piccolo_addDebugLib(&engine);

    struct piccolo_Package* package = piccolo_loadPackage(&engine, arg_info.package);
    if(package->compilationError) {
        piccolo_freeEngine(&engine);
        return -1;
    }

    if(!piccolo_executePackage(&engine, package)) {
        piccolo_enginePrintError(&engine, "Runtime error.\n");
    }

    piccolo_freeEngine(&engine);

    return 0;
}
