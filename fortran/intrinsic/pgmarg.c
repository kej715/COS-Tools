#include <string.h>

static int argCount;
static char **args;

void _setarg(int argc, char *argv[]) {
    argCount = argc;
    args = argv;
}

int _argc(void) {
    return argCount;
}

unsigned long _argv(unsigned long waddr) {
    char *arg;
    long idx;

    idx = *((long *)(waddr << 3));
    arg = args[idx - 1];
    return (((unsigned long)strlen(arg)) << 32) | ((unsigned long)arg);
}
