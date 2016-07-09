#include <uth.h>

#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>

int do_task(int n)
{
    long t = madm::tick();

    while (madm::tick() - t < n * 1000)
        madm::poll();

    return n;
}

int measure()
{
    size_t times = 1000;
#if 0
    // warmup
    for (auto i = 0 ; i < times; i++) {
        madm::future<int> f(do_nothing, 0);
        f.touch();
    }
#endif
    long t0 = madm::tick();

    for (auto i = 0 ; i < times; i++) {
        madm::future<int> f(do_task, 1000);
        f.touch();
    }

    long t1 = madm::tick();

    printf("cycles per task = %ld\n", (t1 - t0) / times);

    return 0;
}

void real_main(int argc, char **argv)
{
    madm::pid_t me = madm::get_pid();

    if (me == 0) {
        madm::future<int> f(measure);
        f.touch();
    }
    madm::barrier();
}

int main(int argc, char **argv)
{
    madm::start(real_main, argc, argv);
    return 0;
}

