#ifndef UTH_CXX_H
#define UTH_CXX_H

#include "uth/uth_comm.h"
#include "uth/uth_options.h"
#include <madm_comm.h>
#include <cstddef>

namespace madm {

    typedef madi::pid_t pid_t;

    template <class... Args>
    void start(void (*f)(int argc, char ** argv, Args... args),
               int& argc, char **& argv, Args... args);

    pid_t get_pid();
    size_t get_n_procs();

    void barrier();

    void poll();

    long tick();
    double time();

}

#include <vector>

namespace madi {

    struct prof_steal_entry {
        void *ctx;
        long frame_size;
        long me;
        long victim;
        long empty_check;
        long lock;
        long steal;
        long suspend;
        long stack_transfer;
        long unlock;
        long resume;
        long tmp;

        void print(FILE *fp)
        {
            fprintf(fp,
                    "ctx = %p, frame_size = %6ld, me = %3ld, victim = %3ld, "
                    "empty = %9ld, lock = %9ld, steal = %9ld, "
                    "suspend = %9ld, stack = %9ld, unlock = %9ld, "
                    "resume = %9ld\n",
                    ctx, frame_size, me, victim,
                    empty_check, lock, steal,
                    suspend, stack_transfer, unlock,
                    resume);
        }
    };

    struct prof {
        size_t max_stack_usage;

        size_t n_success_steals;
        size_t n_aborted_steals;
        size_t n_failed_steals_lock;
        size_t n_failed_steals_empty;

        size_t steals_size;
        size_t steals_idx;
        std::vector<prof_steal_entry> steals;

    public:
        prof()
            : steals_size(uth_options.profile_enabled ? 16 * 1024 : 1)
            , steals_idx(0)
            , steals(steals_size)
        {
            // touch
            memset(steals.data(), 0, sizeof(prof_steal_entry) * steals.size());
        }

        prof_steal_entry& current_steal()
        {
            return steals[steals_idx];
        }

        void next_steal()
        {
#if 0
            if (current_steal().suspend != 0)
                current_steal().print(stderr);
#endif
            if (uth_options.profile_enabled)
                if (steals_idx + 1 < steals_size)
                    if (current_steal().steal != 0)
                        steals_idx++;
        }

        void output()
        {
            if (uth_options.profile_enabled) {

                // calculate max value of stack usage
                size_t stack_usage = 0;
                madi::comm::reduce(&stack_usage, &max_stack_usage, 1, 0,
                                   madi::comm::reduce_op_max);

                // calculate # of steals
                size_t all_success_steals = 0;
                madi::comm::reduce(&all_success_steals, &n_success_steals,
                                   1, 0, madi::comm::reduce_op_sum);

                size_t all_aborted_steals = 0;
                madi::comm::reduce(&all_aborted_steals, &n_aborted_steals,
                                   1, 0, madi::comm::reduce_op_sum);

                size_t all_failed_steals_lock = 0;
                madi::comm::reduce(&all_failed_steals_lock,
                                   &n_failed_steals_lock,
                                   1, 0, madi::comm::reduce_op_sum);

                size_t all_failed_steals_empty = 0;
                madi::comm::reduce(&all_failed_steals_empty,
                                   &n_failed_steals_empty,
                                   1, 0, madi::comm::reduce_op_sum);

                size_t all_failed_steals = all_aborted_steals
                                         + all_failed_steals_lock
                                         + all_failed_steals_empty;
                size_t all_steals = all_success_steals + all_failed_steals;

                int me = ::madm::get_pid();
                if (me == 0) {
                    printf("stack_usage = %zu,\n"
                           "n_steals = %zu, n_success_steals = %zu, "
                           "n_failed_steals = %zu, \n"
                           "n_aborted_steals = %zu, "
                           "n_failed_steals_lock = %zu "
                           "n_failed_steals_empty = %zu\n",
                           stack_usage,
                           all_steals, all_success_steals,
                           all_failed_steals,
                           all_aborted_steals,
                           all_failed_steals_lock,
                           all_failed_steals_empty);
                }

                char fname[1024];
                sprintf(fname, "steal.%03ld.out", madm::get_pid());

                FILE *fp = fopen(fname, "w");

                for (size_t i = 0; i < steals_idx; i++) {
                    prof_steal_entry& e = steals[i];
                    e.print(fp);
                }

                fclose(fp);
            }
        }
    };

    extern prof *g_prof;
}

#include "uth/future-inl.h"
#include "uth/uth-inl.h"
#include "uth/debug.h"

#endif
