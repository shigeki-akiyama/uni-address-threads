#ifndef MADI_PROCESS_INL_H
#define MADI_PROCESS_INL_H

#include "process.h"
#include "uth_config.h"
#include "debug.h"

namespace madi {
    
    inline bool process::initialized() { return initialized_; }

    inline uth_comm& process::com() { return comm_; }

    inline iso_space& process::ispace() { return ispace_; }

    inline FILE * process::debug_out() { return debug_out_; }
    
    inline worker& process::worker_from_id(size_t id)
    {
        MADI_ASSERT(id == 0);
        return workers_[id];
    }
  
    bool process_do_initialize(process& self, int& argc, char **& argv);
    void process_do_finalize(process& self);

    namespace {
        template <class... Args>
        void process_start(int argc, char **argv,
                           std::reference_wrapper<process> ref,
                           void (*f)(int, char **, Args...),
                           Args... args)
        {
            process& self = ref;

            // FIXME
            process_do_initialize(self, argc, argv);

            self.workers_[0].start(f, argc, argv);

            process_do_finalize(self);
        }
    }

    template <class... Args>
    inline void process::start(void (*f)(int, char **, Args...),
                               int& argc, char **& argv, Args... args)
    {
        comm_.start(process_start, argc, argv, std::ref(*this), f, args...);
    }
}

#endif
