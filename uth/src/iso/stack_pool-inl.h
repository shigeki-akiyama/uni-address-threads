#ifndef MADI_ISO_STACK_STACK_POOL_INL_H
#define MADI_ISO_STACK_STACK_POOL_INL_H

#include "iso_space.h"
#include "process-inl.h"

namespace madi {

    inline void * stack_allocator::fill_pool(size_t size, size_t n_units)
    {
        process& proc = madi::proc();
        iso_space& ispace = proc.ispace();

        if (size <= 1 * 1024 * 1024) {
            void *p_chunk = ispace.allocate(size * n_units);

            if (p_chunk == NULL)
                madi::die("stack memory segment is full");

            MADI_DPUTS2("ispace.allocate(%zu) = [%p, %p)", 
                        size * n_units,
                        p_chunk,
                        (char *)(p_chunk) + size * n_units);

            uint8_t *chunk = reinterpret_cast<uint8_t *>(p_chunk);

            size_t i = 0;
            uint8_t *p = chunk + size * i++;

            for (; i < n_units; i++) {
                pool_.push(reinterpret_cast<void *>(chunk + size * i),
                           size);
            }

            return reinterpret_cast<void *>(p);
        } else {
            void *p = ispace.allocate(size);

            if (p == NULL)
                madi::die("stack memory segment is full");

            return p;
        }
    }

    inline void * stack_allocator::allocate(size_t size)
    {
        size_t n_alloc_units = 64;

        void *p = pool_.pop(size);

        if (p == NULL)
            p = fill_pool(size, n_alloc_units);

        return p;
    }

    inline void stack_allocator::deallocate(void *p, size_t size)
    {
        pool_.push(p, size);
    }

}

#endif
