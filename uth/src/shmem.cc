#include "uth_comm_shmem-inl.h"

#include "uth_comm-inl.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <cerrno>

using namespace madi;

static void * mmap_shared_file(void *addr, size_t size, madi::pid_t pid,
                               bool creat)
{
    MADI_CHECK((uintptr_t)addr % 4096 == 0);
    MADI_CHECK(size % 4096 == 0);

    int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    int oflag = O_RDWR;

    if (creat)
        oflag |= O_CREAT;

    char fname[NAME_MAX];
    sprintf(fname, "/massivethreadsdm.%d", (int)pid);

    if (creat) {
        int r = shm_unlink(fname);

        if (r != 0 && errno != ENOENT) {
            perror("shm_unlink");
            MADI_CHECK(r == 0);
        }
    }

    int fd = shm_open(fname, oflag, mode);

    if (fd < 0) {
        perror("shm_open");
        MADI_CHECK(fd >= 0);
    }

    if (creat) {
        int r = ftruncate(fd, size);

        if (r != 0) {
            perror("ftruncate");
            MADI_CHECK(r == 0);
        }
    }

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED;
    if (addr != NULL)
        flags |= MAP_FIXED; // FIXME: MAP_FIXED is unsafe

    MADI_DPUTS2("mmap(%p, %zu, %x, %x, %d, 0); pid = %zu",
                addr, size, prot, flags, fd, pid);

    void *ptr = mmap(addr, size, prot, flags, fd, 0);

    if (ptr == MAP_FAILED) {
        perror("mmap");
        MADI_CHECK(ptr != MAP_FAILED);
    }

    if (addr != NULL && ptr != addr) {
        MADI_DPUTS("ptr = %p, addr = %p, size = %zu", ptr, addr, size);
        MADI_CHECK(ptr == addr);
    }

    close(fd);

    return ptr;
}

static void munmap_shared_file(void *addr, size_t size, madi::pid_t pid,
                               bool destroy)
{
    munmap(addr, size);

    if (destroy) {
        char fname[NAME_MAX];
        sprintf(fname, "/massivethreadsdm.%d", (int)pid);

        int r = shm_unlink(fname);

        MADI_CHECK(r == 0);
    }
}

static void do_mmap_shared(uth_comm& c, void *addr, size_t size, void **ptrs)
{
    madi::pid_t me = c.pid();
    size_t n_procs = c.n_procs();

    ptrs[me] = mmap_shared_file(addr, size, me, true);

    c.barrier();

    for (madi::pid_t pid = 0; pid < n_procs; pid++) {
        if (pid != me) {
            ptrs[pid] = mmap_shared_file(NULL, size, pid, false);
        }
    }
}

void ** shmem::reg_mmap_shared(void *addr, size_t size)
{
    size_t n_procs = c_.n_procs();

    void **ptrs = new void *[n_procs];

    do_mmap_shared(c_, addr, size, ptrs);

    return ptrs;
}

void shmem::reg_munmap_shared(void **ptrs, void *addr, size_t size)
{
    madi::pid_t me = c_.pid();
    size_t n_procs = c_.n_procs();

    for (madi::pid_t pid = 0; pid < n_procs; pid++) {
        if (pid != me) {
            munmap_shared_file(ptrs[pid], size, pid, false);
        }
    }

    c_.barrier();

    munmap_shared_file(ptrs[me], size, me, true);

    delete [] ptrs;
}

