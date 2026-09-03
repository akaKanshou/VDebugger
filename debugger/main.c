#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <unistd.h>

#include "debugger.h"
#include "registers.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: VDB [filename/program]\n");
        return 0;
    }

    char *program = argv[1];

    int pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Fork failed with error %i\n", errno);
        return 1;
    }

    if (pid == 0) {
        // child process

        ptrace(PT_TRACE_ME, NULL, NULL, NULL);
        personality(ADDR_NO_RANDOMIZE);
        execl(program, program, NULL);
    }

    // parent process
    loadDWARFMappings();

    Debugger *dbg = newDebugger(pid);
    int err = runDebugger(dbg);
    if (err) {
        fprintf(stderr, "Encountered an error: %i", err);
        return 1;
    }

    return 0;
}