#include <stdio.h>
#include "piccolo/include.h"
#include "piccolo/stdlib/stdlib.h"

void printError(const char* format, va_list args) {
    vfprintf(stderr, format, args);
}

int main(int argc, const char** argv) {
    if(argc != 2) {
        fprintf(stderr, "Usage: piccolo [filepath]\n");
        return 1;
    }

    struct piccolo_Engine engine;
    piccolo_initEngine(&engine, printError);
    piccolo_addIOLib(&engine);
    piccolo_addTimeLib(&engine);

    struct piccolo_Package* package = piccolo_loadPackage(&engine, argv[1]);
    if(package->compilationError) {
        piccolo_freeEngine(&engine);
        return -1;
    } else if(!piccolo_executePackage(&engine, package)) {
        piccolo_enginePrintError(&engine, "Runtime error.\n");
    }

    piccolo_freeEngine(&engine);

    return 0;
}
