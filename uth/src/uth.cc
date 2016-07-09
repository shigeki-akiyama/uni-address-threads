
#include "uth.h"
#include "madi.h"
#include "process.h"

namespace madm {

    madm::pid_t get_pid() { return madi::proc().com().get_pid(); }
    size_t get_n_procs() { return madi::proc().com().get_n_procs(); }

    void barrier() { madi::barrier(); }
}
