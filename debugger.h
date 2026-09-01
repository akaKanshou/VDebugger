#ifndef DEBUGGER_H
#define DEBUGGER_h

#include <stdlib.h>

typedef struct debugger {
    int c_pid;
} debugger;

debugger* initDebugger(int c_pid);

int debugStart(debugger *dbg);

int debugContinue(debugger *dbg);

#endif

