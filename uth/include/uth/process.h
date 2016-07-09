#ifndef MADI_PROCESS_H
#define MADI_PROCESS_H

#include <cstdio>
#include "uni/taskq.h"
#include "uni/worker.h"
#include "uth_comm.h"
#include "iso_space.h"
#include "misc.h"

#include <vector>

namespace madi {

    class process : noncopyable {
    public:
        bool initialized_;
        uth_comm comm_;
        iso_space ispace_;
        FILE *debug_out_;

        //std::vector<worker> workers_;
        worker workers_[1];

    public:
        process();
        ~process();
      
        bool initialize(int& argc, char **& argv);
        void finalize();

        template <class... Args>
        void start(void (*f)(int argc, char **argv, Args... args),
                   int& argc, char **& argv, Args... args);
        
        bool initialized();
        uth_comm& com();
        iso_space& ispace();
        FILE *debug_out();
        worker& worker_from_id(size_t id);
    };
    
}

#endif

