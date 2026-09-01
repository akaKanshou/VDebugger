#include "debugger.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>

debugger *initDebugger(int c_pid)
{
    debugger *dbg = (debugger *)malloc(sizeof(debugger));
    dbg->c_pid = c_pid;
    return dbg;
}

int debugStart(debugger *dbg)
{
    int status, options = 0;
    waitpid(dbg->c_pid, &status, options);
}

int debugContinue(debugger *dbg)
{
    ptrace(PTRACE_CONT, dbg->c_pid, NULL, NULL);

    int status, options = 0;
    waitpid(dbg->c_pid, &status, options);
}