#ifndef MADI_ISO_STACK_STACK_POOL_H
#define MADI_ISO_STACK_STACK_POOL_H

#include "madi.h"
#include "debug.h"
#include <cstddef>

namespace madi {

    class stack_pool {
//        MADI_NONCOPYABLE(stack_pool);

        enum constants {
            MAX_STACK_BITS = 30,
        };

        void *freelist_[MAX_STACK_BITS];

        size_t index_of_size(size_t size)
        {
            return 64UL - static_cast<size_t>(__builtin_clzl(size - 1));
        }

    public:
        stack_pool()
        {
            for (size_t i = 0; i < MAX_STACK_BITS; i++)
                freelist_[i] = NULL;
        }

        ~stack_pool()
        {
        }

        stack_pool(const stack_pool& p)
        {
            for (size_t i = 0; i < MAX_STACK_BITS; i++)
                freelist_[i] = p.freelist_[i];
        }

        stack_pool& operator=(const stack_pool& p) { MADI_NOT_REACHED; }

        void push(void *stack, size_t size)
        {
            size_t idx = index_of_size(size);

            void *next = freelist_[idx];

            uint8_t *p = reinterpret_cast<uint8_t *>(stack);
            void **stack_base = reinterpret_cast<void **>(p + size);

            stack_base[-1] = next;

            freelist_[idx] = reinterpret_cast<void *>(stack_base);
        }

        void * pop(size_t size)
        {
            size_t idx = index_of_size(size);

            void **stack_base = reinterpret_cast<void **>(freelist_[idx]);

            if (stack_base == NULL) {
                return NULL;
            } else {
                void *next = stack_base[-1];
                freelist_[idx] = next;

                uint8_t *stack =
                    reinterpret_cast<uint8_t *>(stack_base) - size;

                return reinterpret_cast<void *>(stack);
            }
        }

    };

    class stack_allocator {
        stack_pool pool_;

        void * fill_pool(size_t size, size_t n_units);
    public:
        stack_allocator() : pool_() {}
        ~stack_allocator() {}

        void * allocate(size_t size);
        void deallocate(void *p, size_t size);
    };

}

#endif
