#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

void cli_parseArgs(const int argc, const char* const restrict* restrict const argv, const cli_argHandler handler, const size_t nShortArgs, const char* restrict shortArgs, const size_t nLongArgs, const char* const restrict* const restrict longArgs, void* const restrict passthrough) {
	if(!argc) return;
	if(!argv) return;
	if(argc < 0) return;
	if(!handler) return;
	if(nShortArgs && !shortArgs) return;
	if(nLongArgs && !longArgs) return;

	// Precalculating the long args' lengths to save time while looping
	// We need this weirdness to avoid creating a zero-length VLA
	size_t longArgLens[nLongArgs ? nLongArgs : 1];
	if(nLongArgs) {
		for(register size_t i = 0; i < nLongArgs; ++i) {
			longArgLens[i] = 0;
			while(longArgs[i][longArgLens[i]]) longArgLens[i]++;
		}
	}

	for(register size_t i = 0; i < argc - 1; i++) {
        const char const* arg = (argv + 1)[i];

		cli_argType type;
		size_t argn = SIZE_MAX;
		const char* value = NULL;

		if(arg[0] == '-') {
			if(arg[1] == '-' && nLongArgs) {
				type = CLI_ARG_LONG;
				for(register size_t j = 0; j < nLongArgs; ++j) {
					const size_t argLen = longArgLens[j];

					// Calculating inline like this might be very slightly faster
					// given the specific use-case
					for(register size_t k = 0; k < argLen; ++k) {
						if(longArgs[j][k] != arg[k + 2]) goto longArgContinue;
					}

					argn = j;
					if(arg[argLen + 2]) value = arg + 2 + argLen + 1;

				longArgContinue:
					continue;
				}
			}
			else if(nShortArgs) {
				type = CLI_ARG_SHORT;
				for(register size_t j = 0; j < nShortArgs; ++j) {
					if(shortArgs[j] != arg[1]) continue;

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
static char shortArgs[] = { 'd' };
// --fizz --buzz style args
static char* longArgs[] = { "debug", "pkg-path" };

static void handler(const cli_argType type, const size_t nArg, const char* const restrict arg, void* const restrict passthrough) {
    struct cli_argInfo* const argInfo = passthrough;

    switch(type) {
        case CLI_ARG_SHORT: {
            if(nArg == 0) argInfo->debug = true;
            break;
        }
        case CLI_ARG_LONG: {
            if(nArg == 0) argInfo->debug = true;
            else if(nArg == 1) argInfo->path = arg;
            break;
        }
        case CLI_ARG_RAW: {
            if(argInfo->package) {
                fprintf(stderr, "Passing multiple packages is not supported");
                exit(EXIT_FAILURE);
            }
            argInfo->package = arg;
            break;
        }
    }
}

int main(int argc, const char** argv) {
    struct cli_argInfo argInfo = { 0 };
    cli_parseArgs(argc, argv, handler, sizeof(shortArgs) / sizeof(shortArgs[0]), shortArgs, sizeof(longArgs) / sizeof(longArgs[0]), longArgs, &argInfo);

    if(argInfo.debug) {
        printf("Package = %s\n", argInfo.package);
        printf("Debug = %s\n", argInfo.debug ? "true" : "false");
        printf("Search Path = %s\n", argInfo.path);
        
    }

    if(!argInfo.package) {
        fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct piccolo_Engine engine;
    piccolo_initEngine(&engine, printError);
    piccolo_addIOLib(&engine);
    piccolo_addTimeLib(&engine);
    piccolo_addMathLib(&engine);
    if(argInfo.debug)
        piccolo_addDebugLib(&engine);

    struct piccolo_Package* package = piccolo_loadPackage(&engine, argInfo.package);
    if(package->compilationError) {
        piccolo_freeEngine(&engine);
        return EXIT_FAILURE;
    }

    if(!piccolo_executePackage(&engine, package)) {
        piccolo_enginePrintError(&engine, "Runtime error.\n");
    }

    piccolo_freeEngine(&engine);

    return EXIT_SUCCESS;
}
