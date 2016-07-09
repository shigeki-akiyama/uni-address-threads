#ifndef MADI_FJRDMA_H
#define MADI_FJRDMA_H

#include "madi.h"
#include <cstddef>

namespace madi {

    class uth_comm;

    class fjrdma {
    public:
        fjrdma(uth_comm& c) {}
        ~fjrdma() {}

        bool initialize();
        void finalize();

        void **reg_mmap_shared(void *addr, size_t size);
        void reg_munmap_shared(void **ptrs, void *addr, size_t size);

        void reg_put(void *dst, void *src, size_t size, madi::pid_t target);
        void reg_get(void *dst, void *src, size_t size, madi::pid_t target);
    };

}

#endif
