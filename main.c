#include <stdlib.h>
#include <stdio.h>

#include <errno.h>
#include <unistd.h>
#include <sys/ptrace.h>

#include "debugger.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: VDB [filename/program]\n");
        return 0;
    }

    char *program = argv[1];

    int pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "Fork failed with error %i\n", errno);
        return 1;
    }

    if (pid == 0)
    {
        // child process

        printf("Entered child proccess: %s\n", program);

        ptrace(PT_TRACE_ME, NULL, NULL, NULL);
        execl(program, program, NULL);
    }

    // parent process
    printf("Entered parent proccess\n");
    debugger *dbg = initDebugger(pid);
    debugStart(dbg);

    char c;
    scanf("%c", &c);

    debugContinue(dbg);
}