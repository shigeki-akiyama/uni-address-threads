#ifndef MADI_H
#define MADI_H

#include <cstdio>
#include "misc.h"

namespace madi {

    class process;
    class worker;
    class iso_space;

    typedef size_t pid_t;

    bool initialized();
    void exit(int exitcode) MADI_NORETURN;

    void barrier();
    void native_barrier();

    process& proc();
    worker& current_worker();

    FILE *debug_out();

    iso_space& get_iso_space();
}

#endif
