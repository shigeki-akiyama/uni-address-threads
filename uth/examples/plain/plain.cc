#include <uth.h>

#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>

double t0, t1, t2;

void real_main(int argc, char **argv)
{
    MADI_DPUTS("start");

    t1 = madm::time();

    madm::barrier();

    t2 = madm::time();

    auto me = madm::get_pid();
    auto n_procs = madm::get_n_procs();

    if (me == 0) {
        printf("n_procs = %5zu, init = %9.6f, barrier = %9.6f, ",
               n_procs, t1 - t0, t2 - t1);
    }

}

int main(int argc, char **argv)
{
    t0 = madm::time();

    madm::start(real_main, argc, argv);

    return 0;
}

