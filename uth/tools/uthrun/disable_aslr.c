
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>

/* this file used to use MADI_OS_Linux to
   conditionally compile on Linux;
   change to using constants.h */
#include "uth_config.h"

#if MADI_OS_TYPE == MADI_OS_LINUX
#include <sys/personality.h>
#endif

#if defined __sparc__ && defined __arch64__  // for FX10
#define ADDR_COMPAT_LAYOUT  (0x0)
#endif


int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [program [program arguments]]\n", argv[0]);
        exit(EX_USAGE);
    }

#if MADI_OS_TYPE == MADI_OS_LINUX
    unsigned int flags = PER_LINUX | ADDR_NO_RANDOMIZE | ADDR_COMPAT_LAYOUT;
    if (personality(flags) == -1) {
        perror("personality");
        exit(1);
    }
#endif

    argv += 1;
    execvp(argv[0], argv);
    return 0;
}
