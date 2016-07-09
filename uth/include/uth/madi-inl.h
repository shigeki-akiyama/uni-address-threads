#ifndef MADI_INL_H
#define MADI_INL_H

#include "madi.h"
#include "uth_comm-inl.h"
#include "process-inl.h"

namespace madi {

    extern process madi_process;
    extern /*volatile __thread*/ size_t madi_worker_id;

    inline process& proc() { return madi_process; }

    inline worker& current_worker()
    {
        return madi_process.worker_from_id(madi_worker_id);
    }

    inline bool initialized() { return madi::proc().initialized(); }
}

#endif
