#include <uth.h>

#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>

int do_nothing(int i)
{
    return i;
}

int do_nothing_noinline(int i);

int measure()
{
    size_t times = 1000 * 1000;
#if 0
    // warmup
    for (auto i = 0 ; i < times; i++) {
        madm::future<int> f(do_nothing, 0);
        f.touch();
    }
#endif
    {
        long t0 = madm::tick();
        for (auto i = 0 ; i < times; i++) {
            madm::future<int> f(do_nothing, 0);
            f.touch();
        }
        long t1 = madm::tick();

        printf("tasking overhead = %ld\n", (t1 - t0) / times);
    }

    {
        long t0 = madm::tick();
        for (auto i = 0; i < times; i++) {
            madm::future<int> f([=]() {
                return i;
            });
            f.touch();
        }
        long t1 = madm::tick();

        printf("tasking overhead (lambda) = %ld\n", (t1 - t0) / times);
    }

    {
        long t0 = madm::tick();
        for (auto i = 0; i < times; i++) {
            do_nothing_noinline(0);
        }
        long t1 = madm::tick();

        printf("function call overhead = %ld\n", (t1 - t0) / times);
    }

    return 0;
}

void real_main(int argc, char **argv)
{
    madm::future<int> f(measure);
    f.touch();
}

int main(int argc, char **argv)
{
    madm::start(real_main, argc, argv);
    return 0;
}

