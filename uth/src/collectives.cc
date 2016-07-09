#include "collectives.h"
#include "uth_comm-inl.h"
#include "comm_armci.h"

using namespace madi;

collectives::collectives(uth_comm& c) :
    c_(c),
    bufs_(NULL),
    parent_(0), parent_idx_(-1),
    barrier_state_(0),
    phase_(0), phase0_idx_(0)
{
    int me = armci_msg_me();
    size_t n_procs = armci_msg_nproc();

    bufs_ = new volatile int *[n_procs];

    size_t n_elems = 2 * 2;
    ARMCI_Malloc((void **)bufs_, sizeof(int) * n_elems);

    for (size_t i = 0; i < n_elems; i++)
        bufs_[me][i] = 0;

    parent_      = (me == 0) ? -1 : (me - 1) / 2; 
    parent_idx_   = (me == 0) ? -1 : (me - 1) % 2;
    children_[0] = 2 * me + 1;
    children_[1] = 2 * me + 2;

    armci_msg_barrier();
}

collectives::~collectives()
{
    int me = armci_msg_me();

    armci_msg_barrier();

    ARMCI_Free((void *)bufs_[me]);

    delete [] bufs_;
}

bool collectives::barrier_try()
{
    int me = (int)c_.get_pid();
    int n_procs = (int)c_.get_n_procs();

    int base = 2 * barrier_state_;

    // reduce
    if (phase_ == 0) {
        for (int i = phase0_idx_; i < 2; i++) {
            if (children_[i] < n_procs) {
                if (bufs_[me][base + i] != 1) {
                    phase_ = 0;
                    phase0_idx_ = i;
                    return false;
                }
            }
        }

        if (parent_ != -1) {
            c_.put_value((int *)&bufs_[parent_][base + parent_idx_], 1, 
                         (madi::pid_t)parent_);
        }

        phase_ += 1;
    }

    // broadcast
    if (phase_ == 1) {
        if (parent_ != -1) {
            if (bufs_[me][base + 0] != 2) {
                phase_ = 1;
                return false;
            }
        }

        for (int i = 0; i < 2; i++) {
            int child = children_[i];
            if (child < n_procs) {
                c_.put_value((int *)&bufs_[child][base + 0], 2,
                             (madi::pid_t)child);
            }
        }
    }

    phase_ = 0;
    phase0_idx_ = 0;

    bufs_[me][base + 0] = 0;
    bufs_[me][base + 1] = 0;

    barrier_state_ = !barrier_state_;

    return true;
}

void collectives::barrier()
{
    while (!barrier_try())
        ;
}

