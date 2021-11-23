#include <stdio.h>
#include <string.h>

#define PICCOLO_ENABLE_DEBUG_LIB

#include "piccolo/include.h"
#include "piccolo/stdlib/stdlib.h"
#include "piccolo/debug/disassembler.h"

void printError(const char* format, va_list args) {
    vfprintf(stderr, format, args);
}

int main(int argc, const char** argv) {
    if(argc == 1) {
        fprintf(stderr, "Usage: piccolo [filepath]\n");
        return 1;
    }

    bool debug = false;
    bool error = false;
    for(int i = 2; i < argc; i++) {
        if(strcmp(argv[i], "-debug") == 0) {
            debug = true;
        } else {
            fprintf(stderr, "Unknown flag %s\n", argv[i]);
            error = true;
        }
    }
    if(error) {
        return 1;
    }

    struct piccolo_Engine engine;
    piccolo_initEngine(&engine, printError);
    piccolo_addIOLib(&engine);
    piccolo_addTimeLib(&engine);
    piccolo_addMathLib(&engine);
    if(debug)
        piccolo_addDebugLib(&engine);

    struct piccolo_Package* package = piccolo_loadPackage(&engine, argv[1]);
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
