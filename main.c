#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>

#include "piccolo/include.h"
#include "piccolo/stdlib/picStdlib.h"
#include "piccolo/debug/disassembler.h"
#include "debugLib.h"

void printError(const char* format, va_list args) {
    vfprintf(stderr, format, args);
}

const char* homePath;

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
        const char* const /* const on wrong side of pointer */ arg = (argv + 1)[i];

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
    size_t n_paths;
    const char** paths;
};


// -f -b style args
static const /* We want this to exist in a constants section */ char shortArgs[] = { 'd' };
// --fizz --buzz style args
static const /* We want this to exist in a constants section */ char* const longArgs[] = { "debug", "pkg-path" };

static void handler(const cli_argType type, const size_t nArg, const char* const restrict arg, void* const restrict passthrough) {
    struct cli_argInfo* const argInfo = passthrough;

    switch(type) {
        case CLI_ARG_SHORT: {
            if(nArg == 0) argInfo->debug = true;
            break;
        }
        case CLI_ARG_LONG: {
            if(nArg == 0) argInfo->debug = true;
            else if(nArg == 1) {
                argInfo->paths = realloc(argInfo->paths, ++argInfo->n_paths * sizeof(char*));
                argInfo->paths[argInfo->n_paths - 1] = arg;
            }
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

struct piccolo_Package* findGitPkg(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, const char* sourceFilepath, const char* name, size_t nameLen) {
    if(nameLen < 4)
        return NULL;
    if(memcmp(name + nameLen - 4, ".git", 4) != 0)
        return NULL;

    const char* pkgNameEnd = name + nameLen - 4;
    char pkgNameBuf[PICCOLO_MAX_PACKAGE];
    char* curr = pkgNameBuf;
    for(const char* i = name; i < pkgNameEnd; ++i) {
        if(*i != '/' && *i != ':') {
            *curr = *i;
            curr++;
        }
    }
    *curr = '\0';

    char pathBuf[PICCOLO_MAX_PACKAGE];
    snprintf(pathBuf, PICCOLO_MAX_PACKAGE, "%s/.piccolo/%s", homePath, pkgNameBuf);
    DIR* dir = opendir(pathBuf);
    if(dir == NULL) {
        char cmdBuf[PICCOLO_MAX_PACKAGE];
        snprintf(cmdBuf, PICCOLO_MAX_PACKAGE, "git clone %.*s --recursive %s &> /dev/null", nameLen, name, pathBuf);
        if(system(cmdBuf)) {
            piccolo_enginePrintError(engine, "Could not clone package '%.*s'.\n", nameLen, name);
            return NULL;
        }
    } else {
        closedir(dir);
    }

    snprintf(pathBuf, PICCOLO_MAX_PACKAGE, "%s/lib.pic", pkgNameBuf);
    return piccolo_resolvePackage(engine, compiler, sourceFilepath, pathBuf, strlen(pathBuf));
}

int main(int argc, const char** argv) {
    struct cli_argInfo argInfo = { 0 };
    cli_parseArgs(argc, argv, handler, sizeof(shortArgs) / sizeof(shortArgs[0]), shortArgs, sizeof(longArgs) / sizeof(longArgs[0]), longArgs, &argInfo);

    homePath = getenv("HOME");

    if(argInfo.debug) {
        printf("Package = %s\n", argInfo.package);
        printf("Debug = %s\n", argInfo.debug ? "true" : "false"); // this is redundant, but ok XD
        for(register size_t i = 0; i < argInfo.n_paths; ++i) {
            printf("Search Path = %s\n", argInfo.paths[i]);
        }
    }

    if(!argInfo.package) {
        fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct piccolo_Engine engine;
    piccolo_initEngine(&engine, printError);

    engine.findPackage = findGitPkg;
    char pathBuf[PICCOLO_MAX_PACKAGE];
    snprintf(pathBuf, PICCOLO_MAX_PACKAGE, "%s/.piccolo/", homePath);
    piccolo_addSearchPath(&engine, pathBuf);

    for(register size_t i = 0; i < argInfo.n_paths; ++i) {
        piccolo_addSearchPath(&engine, argInfo.paths[i]);
    }
    piccolo_addIOLib(&engine);
    piccolo_addTimeLib(&engine);
    piccolo_addMathLib(&engine);
    piccolo_addRandomLib(&engine);
    piccolo_addStrLib(&engine);
    piccolo_addFileLib(&engine);
    piccolo_addDLLLib(&engine);
    piccolo_addOSLib(&engine);
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

    if(argInfo.debug) {
        printf("Live memory: %lu\n", engine.liveMemory);
    }

    return EXIT_SUCCESS;
}
