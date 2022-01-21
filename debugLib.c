
#include "debugLib.h"
#include "piccolo/debug/disassembler.h"
#include "piccolo/embedding.h"
#include <stdio.h>

static piccolo_Value disassembleFunctionNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    } else {
        piccolo_Value val = args[0];
        if(!PICCOLO_IS_OBJ(val) || PICCOLO_AS_OBJ(val)->type != PICCOLO_OBJ_CLOSURE) {
            piccolo_runtimeError(engine, "Cannot dissasemble %s.", piccolo_getTypeName(val));
        } else {
            struct piccolo_ObjClosure* function = (struct piccolo_ObjClosure*) PICCOLO_AS_OBJ(val);
            piccolo_disassembleBytecode(&function->prototype->bytecode);
        }
    }
    return PICCOLO_NIL_VAL();
}

static int assertions = 0;
static int assertionsMet = 0;

static piccolo_Value assertNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args, struct piccolo_Value self) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    } else {
        piccolo_Value val = args[0];
        if(!PICCOLO_IS_BOOL(val)) {
            piccolo_runtimeError(engine, "Expected assertion to be a boolean.");
        } else {
            assertions++;
            bool assertion = PICCOLO_AS_BOOL(val);
            if(assertion) {
                printf("\x1b[32m[OK]\x1b[0m ASSERTION MET\n");
                assertionsMet++;
            } else {
                printf("\x1b[31m[ERROR]\x1b[0m ASSERTION FAILED\n");
            }
        }
    }
    return PICCOLO_NIL_VAL();
}

static piccolo_Value printAssertionResultsNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
    if(argc != 0) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    } else {
        if(assertionsMet == assertions) {
            printf("\x1b[32m%d / %d ASSERTIONS MET! ALL OK\x1b[0m\n", assertionsMet, assertions);
        } else {
            printf("\x1b[31m%d / %d ASSERTIONS MET.\x1b[0m\n", assertionsMet, assertions);
        }
    }
    return PICCOLO_NIL_VAL();
}

void piccolo_addDebugLib(struct piccolo_Engine* engine) {
    struct piccolo_Package* debug = piccolo_createPackage(engine);
    debug->packageName = "debug";
    piccolo_defineGlobal(engine, debug, "disassemble", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, disassembleFunctionNative)));
    piccolo_defineGlobal(engine, debug, "assert", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, assertNative)));
    piccolo_defineGlobal(engine, debug, "printAssertionResults", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, printAssertionResultsNative)));
}
