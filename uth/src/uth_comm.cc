#include "uth_comm.h"
#include "uth_comm-inl.h"

#if MADI_USE_MCOMM

#include <madm_comm.h>

namespace madi {

    bool uth_comm::initialize(int& argc, char **&argv)
    {
        comm::initialize(argc, argv);

        buffer_size_ = 8192;

        buffer_ = comm::rma_malloc<uint8_t>(buffer_size_);

        initialized_ = true;

        return true;
    }

    void uth_comm::finalize()
    {
        comm::rma_free(buffer_);
        comm::finalize();

        buffer_size_ = 0;
        buffer_ = NULL;
    }

    void uth_comm::exit(int exitcode)
    {
        comm::exit(exitcode);
    }

    void ** uth_comm::malloc_shared(size_t size)
    {
        void **ptrs = (void **)comm::coll_rma_malloc<uint8_t>(size);

        if (ptrs == NULL) {
            MADI_SPMD_DIE("cannot allocate RMA memory (size = %zu)",
                          size);
        }

        return ptrs;
    }

    void uth_comm::free_shared(void **p)
    {
        comm::coll_rma_free((uint8_t **)p);
    }

    void * uth_comm::malloc_shared_local(size_t size)
    {
        return (void *)comm::rma_malloc<uint8_t>(size);
    }

    void uth_comm::free_shared_local(void *p)
    {
        comm::rma_free((uint8_t *)p);
    }

    void uth_comm::put(void *dst, void *src, size_t size, madi::pid_t target)
    {
        comm::put(dst, src, size, target);
    }

    void uth_comm::get(void *dst, void *src, size_t size, madi::pid_t target)
    {
        comm::get(dst, src, size, target);
    }

    void uth_comm::put_value(int *dst, int value, madi::pid_t target)
    {
        comm::put_value<int>(dst, value, target);
    }

    void uth_comm::put_value(long *dst, long value, madi::pid_t target)
    {
        comm::put_value<long>(dst, value, target);
    }

    int uth_comm::get_value(int *src, madi::pid_t target)
    {
        return comm::get_value<int>(src, target);
    }

    long uth_comm::get_value(long *src, madi::pid_t target)
    {
        return comm::get_value<long>(src, target);
    }

    void uth_comm::swap(int *dst, int *src, madi::pid_t target)
    {
        MADI_UNDEFINED;
    }

    int uth_comm::fetch_and_add(int *dst, int value, madi::pid_t target)
    {
        return comm::fetch_and_add<int>(dst, value, target);
    }

    void ** uth_comm::reg_mmap_shared(void *addr, size_t size)
    {
        // NOTE: this function can be called at a time
        MADI_CHECK(rdma_id_ == -1);

        size_t n_procs = get_n_procs();

        void **ptrs = new void *[n_procs];

        for (size_t i = 0; i < n_procs; i++)
            ptrs[i] = addr;

        rdma_id_ = comm::reg_coll_mmap(addr, size);

        if (rdma_id_ != -1)
            return ptrs;

        // mcomm library does not support mmap with memory registration
        // in GASNet. Instead, use malloc_shared which is implemented as 
        // all allocated addresses are same (if previous allocations are all 
        // done by specifying the same size arguments).

        delete ptrs;

        ptrs = malloc_shared(size);

        // check whether all addresses are same or not
        bool ok = true;
        uint8_t *true_addr = reinterpret_cast<uint8_t *>(ptrs[0]);
        for (size_t i = 1; i < n_procs; i++) {
            if (ptrs[i] != true_addr) {
                ok = false;
                break;
            }
        }

        rdma_id_ = -2;

        if (ok) {
            return ptrs;
        } else {
            free_shared(ptrs);
            return NULL;
        }
    }

    void uth_comm::reg_munmap_shared(void **ptrs, void *addr, size_t size)
    {
        MADI_CHECK(rdma_id_ != -1);

        if (rdma_id_ != -2) {
            comm::reg_coll_munmap(rdma_id_);
            delete ptrs;
        } else {
            free_shared(ptrs);
        }

        rdma_id_ = -1;
    }

    void uth_comm::reg_put(void *dst, void *src, size_t size,
                           madi::pid_t target)
    {
        MADI_ASSERT(rdma_id_ != -1);

        comm::reg_put(rdma_id_, dst, src, size, target);
    }

    void uth_comm::reg_get(void *dst, void *src, size_t size,
                           madi::pid_t target)
    {
        MADI_ASSERT(rdma_id_ != -1);

        comm::reg_get(rdma_id_, dst, src, size, target);
    }

    void uth_comm::barrier()
    {
        comm::barrier();
    }

    bool uth_comm::barrier_try()
    {
        return comm::barrier_try();
    }

    void uth_comm::poll()
    {
        comm::poll();
    }

}

#else
//---------------------------------------------------------------------------

#include "uth_comm-inl.h"
#include "collectives.h"
#include "uth_config.h"
#include "debug.h"

#include <cstdlib>
#include "comm_armci.h"

using namespace madi;


bool uth_comm::initialize(int& argc, char **&argv)
{
#if   MADI_COMM_LAYER == MADI_COMM_LAYER_SEQ
    int me = 0;
    int n_procs = 1;
#else
#if   MADI_COMM_LAYER == MADI_COMM_LAYER_FX10
    MPI_Init(&argc, &argv);
    rdma_.initialize();
    int r = ARMCI_Init();
#elif MADI_COMM_LAYER == MADI_COMM_LAYER_SHMEM
    MPI_Init(&argc, &argv);
    rdma_.initialize();
    int r = ARMCI_Init_args(&argc, &argv);
#else
#error ""
#endif

    if (r != 0)
        ARMCI_Error((char *)"ARMCI_Init_args failed", r);

    int me = armci_msg_me();
    int n_procs = armci_msg_nproc();
#endif

    pid_ = me;
    n_procs_ = n_procs;

    initialized_ = true;

    coll_ = new collectives(*this);

    barrier();

    return initialized_;
}

void uth_comm::finalize()
{
    barrier();

    delete coll_;

#if MADI_COMM_LAYER != MADI_COMM_LAYER_SEQ
//    ARMCI_Finalize();
    rdma_.finalize();
    MPI_Finalize();
#endif

    initialized_ = false;
}

void uth_comm::start_with_param(void (*f)(int argc, char **argv, void *p),
                                int argc, char **argv, void *p)
{
    f(argc, argv, p);
}

void uth_comm::exit(int exitcode)
{
#if   MADI_COMM_LAYER == MADI_COMM_LAYER_FX10
    // FIXME: ARMCI for FX10 does not support ARMCI_Initialized.
    int init;
    int r = MPI_Initialized(&init);

    if (r == MPI_SUCCESS && init)
        ARMCI_Cleanup();
#elif MADI_COMM_LAYER == MADI_COMM_LAYER_SHMEM
    if (ARMCI_Initialized())
        ARMCI_Cleanup();
#else
#error ""
#endif

    std::exit(exitcode);
}

void ** uth_comm::malloc_shared(size_t size)
{
    void **p = new void *[n_procs_];

#if MADI_COMM_LAYER == MADI_COMM_LAYER_SEQ
    for (size_t i = 0; i < n_procs_; i++)
        p[i] = malloc(size);
#else
    for (size_t i = 0; i < n_procs_; i++)
        p[i] = NULL;

    int r = ARMCI_Malloc(p, size);

    MADI_CHECK(r == 0);
#endif

    return p;
}

void uth_comm::free_shared(void **p)
{
#if MADI_COMM_LAYER == MADI_COMM_LAYER_SEQ
    for (size_t i = 0; i < n_procs_; i++)
        free(p[i]);
#else
    int r = ARMCI_Free(p[pid_]);

    MADI_CHECK(r == 0);
#endif

    delete [] p;
}

void * uth_comm::malloc_shared_local(size_t size)
{
    return ARMCI_Malloc_local(size);
}

void uth_comm::free_shared_local(void *p)
{
    ARMCI_Free_local(p);
}

void uth_comm::put(void *dst, void *src, size_t size, madi::pid_t target)
{
    ARMCI_Put(src, dst, (int)size, (int)target);
    ARMCI_Fence((int)target);
}

void uth_comm::get(void *dst, void *src, size_t size, madi::pid_t target)
{
    ARMCI_Get(src, dst, (int)size, (int)target);
}

void uth_comm::put_value(int *dst, int value, madi::pid_t target)
{
    ARMCI_PutValueInt(value, dst, (int)target);
    ARMCI_Fence((int)target);
}

void uth_comm::put_value(long *dst, long value, madi::pid_t target)
{
    ARMCI_PutValueLong(value, dst, (int)target);
    ARMCI_Fence((int)target);
}

int uth_comm::get_value(int *src, madi::pid_t target)
{
    return ARMCI_GetValueInt(src, (int)target);
}

long uth_comm::get_value(long *src, madi::pid_t target)
{
    return ARMCI_GetValueLong(src, (int)target);
}

void uth_comm::swap(int *dst, int *src, madi::pid_t target)
{
    ARMCI_Rmw(ARMCI_SWAP, dst, src, 0, (int)target);
    ARMCI_Fence((int)target);
}

int uth_comm::fetch_and_add(int *dst, int value, madi::pid_t target)
{
    int result;

    ARMCI_Rmw(ARMCI_FETCH_AND_ADD,  &result, dst, value, (int)target);
    ARMCI_Fence((int)target);

    return result;
}

void ** uth_comm::reg_mmap_shared(void *addr, size_t size)
{
    return rdma_.reg_mmap_shared(addr, size);
}

void uth_comm::reg_munmap_shared(void **ptrs, void *addr, size_t size)
{
    return rdma_.reg_munmap_shared(ptrs, addr, size);
}

void uth_comm::reg_put(void *dst, void *src, size_t size, madi::pid_t target)
{
    rdma_.reg_put(dst, src, size, target);
}

void uth_comm::reg_get(void *dst, void *src, size_t size, madi::pid_t target)
{
    rdma_.reg_get(dst, src, size, target);
}

void uth_comm::barrier()
{
#if 0
    armci_msg_barrier();
#else
    coll_->barrier();
#endif
}

bool uth_comm::barrier_try()
{
#if 0
    return true;
#else
    return coll_->barrier_try();
#endif
}

#endif

