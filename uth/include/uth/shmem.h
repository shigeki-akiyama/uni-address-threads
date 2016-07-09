#ifndef MADI_COMM_SHMEM_H
#define MADI_COMM_SHMEM_H

#include "madi.h"

#include <cstddef>
#include <cstdint>

namespace madi {

    class uth_comm;

    class shmem {
        MADI_NONCOPYABLE(shmem);

        uth_comm& c_;

        public:
        shmem(uth_comm& c) : c_(c) {}
        ~shmem() {}

        void initialize() {}
        void finalize() {}

        void **reg_mmap_shared(void *addr, size_t size);
        void reg_munmap_shared(void **ptrs, void *addr, size_t size);

        void reg_put(void *dst, void *src, size_t size, madi::pid_t target);
        void reg_get(void *dst, void *src, size_t size, madi::pid_t target);
    };

}

#endif
