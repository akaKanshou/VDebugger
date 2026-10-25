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
        printf("Usage: VDB <program> <cmdline_arguments_to_program>\n");
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
        execvp(argv[1], &argv[1]);
        fprintf(stdout, "Execl Error :%i\n", errno);
        return 1;
    }

    // parent process
    load_DW_mappings();

    int res;
    Debugger *dbg;
    res = new_debugger(pid, program, &dbg);
    int err = run_debugger(dbg);
    if (err) {
        fprintf(stderr, "Encountered an error: %i", err);
        return 1;
    }
    ptrace(PT_DETACH, dbg->c_pid, NULL);
    free_debugger(dbg);

    return 0;
}