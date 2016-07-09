#ifndef MADI_COLLECTIVES_H
#define MADI_COLLECTIVES_H

#include "misc.h"

namespace madi {

    class uth_comm;

    class collectives {
        MADI_NONCOPYABLE(collectives);

        uth_comm& c_;
        volatile int **bufs_; 
        int parent_;
        int parent_idx_;
        int children_[2];
        int barrier_state_;

        int phase_;
        int phase0_idx_;

    public:
        collectives(uth_comm& c);
        ~collectives();
        bool barrier_try();
        void barrier();
    };

}

#endif
