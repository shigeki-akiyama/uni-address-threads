#include "fjrdma.h"
#include "debug.h"
#include <sys/mman.h>

#pragma GCC diagnostic ignored "-Weffc++"
#include <mpi.h>
#include <mpi-ext.h>
#pragma GCC diagnostic warning "-Weffc++"

using namespace madi;

bool fjrdma::initialize()
{
    FJMPI_Rdma_init();
    return true;
}

void fjrdma::finalize()
{
    FJMPI_Rdma_finalize();
}

void ** fjrdma::reg_mmap_shared(void *addr, size_t size)
{
    int i;

    int pid, n_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &n_procs);

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;

    if (addr != NULL)
        flags |= MAP_FIXED;

    void *p = mmap(addr, size, prot, flags, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        abort();
    }

    if (addr != NULL && p != addr) {
        MADI_DPUTS("p = %p, addr = %p, size = %zu", p, addr, size);
        MADI_CHECK(p == addr);
    }

    int memid = 0; // FIXME

    MADI_DPUTS2("FJMPI_Rdma_reg_mem(%d, %p, %zu) call", memid, p, size);

    uint64_t local_addr = FJMPI_Rdma_reg_mem(memid, p, size);

    MPI_Barrier(MPI_COMM_WORLD);

    void **ptrs = new void *[n_procs];

#if 1
    ptrs[pid] = (void *)local_addr;

    for (i = 0; i < n_procs; i++) {
        if (i == pid)
            continue;

        uint64_t remote_addr = FJMPI_Rdma_get_remote_addr((int)i, memid);

        if (remote_addr == FJMPI_RDMA_ERROR) {
            MADI_DPRINT("FJMPI_Rdma_get_remote_addr: error");
            abort();
        }

        ptrs[i] = (void *)remote_addr;
    }
#else
    void *rdma_addr = (void *)FJMPI_Rdma_get_remote_addr(me_, memid);

    MPI_Allgather(&rdma_addr, sizeof(rdma_addr), MPI_BYTE,
                  ptrs, sizeof(rdma_addr) * n_procs_, MPI_BYTE,
                  MPI_COMM_WORLD);
#endif

    return ptrs;
}

void fjrdma::reg_munmap_shared(void **ptrs, void *addr, size_t size)
{
#if 0
    // Fujitsu mpig++ does not provide FJMPI_Rdma_dereg_mem function...
    int memid = 0; // FIXME
    int r = FJMPI_Rdma_dereg_mem(memid);

    if (r != 0) {
        MADI_DPRINT("FJMPI_Rdma_dereg_mem: error");
    }
#endif

    munmap(addr, size);

    delete [] ptrs;
}

void fjrdma::reg_put(void *dst, void *src, size_t size, madi::pid_t target)
{
    int pid = (int)target;
    int tag = 0;
    uint64_t raddr = (uint64_t)dst;
    uint64_t laddr = (uint64_t)src;
    size_t length = size;
    int flags = FJMPI_RDMA_LOCAL_NIC0 | FJMPI_RDMA_REMOTE_NIC0;

    FJMPI_Rdma_put(pid, tag, raddr, laddr, length, flags);
}

void fjrdma::reg_get(void *dst, void *src, size_t size, madi::pid_t target)
{
    int pid = (int)target;
    int tag = 0;
    uint64_t raddr = (uint64_t)src;
    uint64_t laddr = (uint64_t)dst;
    size_t length = size;
    int flags = FJMPI_RDMA_LOCAL_NIC0 | FJMPI_RDMA_REMOTE_NIC0;

    FJMPI_Rdma_get(pid, tag, raddr, laddr, length, flags);
}

