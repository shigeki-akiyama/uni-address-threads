#include "shmem.h"

#include <cstring>

namespace madi {

    void shmem::reg_put(void *dst, void *src, size_t size, madi::pid_t target)
    {
        memcpy(dst, src, size);
    }
   
    void shmem::reg_get(void *dst, void *src, size_t size, madi::pid_t target)
    {
        memcpy(dst, src, size);
    }
}

