
#include "madm_debug.h"
#include "madm_misc.h"
#include <memory>
#include <vector>
#include <cstdint>

namespace madi {
namespace comm {

    struct alc_header {
        alc_header *next;
        uintptr_t size;
    };

    template <class MemRegion>
    class allocator {
        alc_header *free_list_;
        MemRegion *mr_;
        alc_header *header_;

    public:
        allocator(MemRegion *mr);
        ~allocator();

        template <class T>
        void * allocate(size_t size, T& param);
        void deallocate(void *p);

        // custom deleter for allocator<T>::unique_ptr
        template <class T>
        class default_delete {
            allocator<MemRegion> *alc_;
        public:
            explicit default_delete() : alc_(NULL) {}
            explicit default_delete(allocator *alc) : alc_(alc) {}

            void operator()(T *p)
            {
                alc_->deallocate((void *)p);
            }
        };

        // unique_ptr for the memory allocated by allocator<T>
        // (g++ 4.6.1 does not support template aliases)
        template <class T, class D = default_delete<T>>
        struct unique_ptr : std::unique_ptr<T, D> {
            typedef typename std::unique_ptr<T, D>::pointer pointer;

            constexpr unique_ptr() : std::unique_ptr<T, D>() {}
            explicit unique_ptr(pointer p) : std::unique_ptr<T, D>(p) {}
            unique_ptr(pointer p, D d) : std::unique_ptr<T, D>(p, d) {}
            unique_ptr(unique_ptr&& u) : std::unique_ptr<T, D>(u) {}
            constexpr unique_ptr(std::nullptr_t p)
                : std::unique_ptr<T, D>(p) {}

            template <class U, class E>
            unique_ptr(unique_ptr<U, E>&& u) : std::unique_ptr<U, E>(u) {}

            template <class U>
            unique_ptr(std::auto_ptr<U>&& u) : std::unique_ptr<U>(u) {}
        };

        // make_unique for allocator<T>
        template <class T, class... Args>
        unique_ptr<T> make_unique(void *param, Args&&... args)
        {
            void *p = allocate(sizeof(T), param);

            T *obj = new (p) T(std::forward<Args>(args)...);

            return unique_ptr<T>(obj, default_delete<T>(this));
        }
    };


    template <class MR>
    allocator<MR>::allocator(MR *mr) : free_list_(NULL), mr_(mr)
    {
        header_ = new alc_header;
        header_->next = header_;
        header_->size = 0;

        free_list_ = header_;
    }

    template <class MR>
    allocator<MR>::~allocator()
    {
        delete header_;
    }
#if 1

#define MADI_ALC_ASSERT(h) \
    do { \
        MADI_ASSERT((uintptr_t)((h)->next) >= 4096); \
        MADI_ASSERT((h)->size <= 32 * 1024 * 1024); \
    } while (false)

    // K&R malloc
    template <class MR>
    template <class T>
    void * allocator<MR>::allocate(size_t size, T& param)
    {
        const size_t init_size = 2 * 1024 * 1024;

        size_t n_units =
            ((size + sizeof(alc_header) - 1)) / sizeof(alc_header) + 1;

        alc_header *prev = free_list_;
        alc_header *h = prev->next;
        for (;;) {
            if (h->size >= n_units)
                break;

            if (prev == free_list_) {
                size_t prev_size = mr_->size();
                size_t total_size = (prev_size == 0) ? init_size : prev_size*2;

                alc_header *new_header =
                    (alc_header *)mr_->extend_to(total_size, param);

                if (new_header == NULL)
                    return NULL;

                size_t ext_size = mr_->size() - prev_size;
                size_t new_n_units = ext_size / sizeof(alc_header);

                MADI_ASSERT(ext_size % sizeof(alc_header) == 0);

                new_header->next = NULL;
                new_header->size = new_n_units;

                deallocate((void *)(new_header + 1));

                // retry this loop
                prev = free_list_;
            }
           
            prev = h;
            h = h->next;
        }

        if (h->size == n_units) {
            prev->next = h->next;
        } else {
            h->size -= n_units;

            h += h->size;
            h->size = n_units;
        }

        free_list_ = prev;
        h->next = NULL;

        return (void *)(h + 1);
    }

    template <class MR>
    void allocator<MR>::deallocate(void *p)
    {
        alc_header *header = (alc_header *)p - 1;

        alc_header *h = free_list_;
        for (;;) {
            if (h < header && header < h->next)
                break;

            if (h >= h->next && (h < header || header < h->next))
                break;

            h = h->next;
        }

        if (header + header->size == h->next) {
            header->size += h->next->size;
            header->next = h->next->next;
        } else {
            header->next = h->next;
        }

        if (h + h->size == header) {
            h->size += header->size;
            h->next = header->next;
        } else {
            h->next = header;
        }

        free_list_ = h;
    }
#else
    template <class MR>
    void * allocator<MR>::allocate(size_t size)
    {
        if (size == 0)
            return NULL;

        size_t actual_size = sizeof(alc_header) + size;

        alc_header *end_header = free_list_;
        alc_header *prev_header = free_list_;

        MADI_ASSERT(prev_header->next != NULL);

        alc_header *header = prev_header->next;

        while (header != end_header) {
            MADI_ASSERT(header != NULL);
            MADI_ASSERT(header->next != NULL);

            if (header->size >= actual_size)
                break;

            prev_header = header;
            header = header->next;
        }

        if (header == end_header) {
            // extend
            size_t prev_size = mr_->size();
            size_t total_size = mr_->size() * 2;
            header = (alc_header *)mr_->extend_to(total_size);

            MADI_CHECK(header != NULL);

            header->next = end_header;
            header->size = mr_->size() - prev_size - sizeof(header);
        }

        if (header->size == size) {
            prev_header->next = header->next;
            free_list_ = header->next;
        } else {
            alc_header *new_header =
                (alc_header *)((uint8_t *)header + actual_size);

            new_header->next = header->next;
            new_header->size = header->size - actual_size;

            header->size = size;
            prev_header->next = new_header;
            free_list_ = new_header;
        }

        header->next = NULL;
        return (void *)(header + 1);
    }

    inline alc_header *try_merge(alc_header * h0, alc_header * h1)
    {
        uint8_t *h0_end = (uint8_t *)(h0 + 1) + h0->size;

        if (h0_end == (uint8_t *)h1) {
            h0->next = h1->next;
            h0->size += sizeof(*h1) + h1->size;
            return h0;
        } else {
            return h1;
        }
    }

    template <class MR>
    void allocator<MR>::deallocate(void *p)
    {
        if (p == NULL)
            return;

        alc_header *h = (alc_header *)p - 1;

        alc_header *end_header = free_list_;
        alc_header *prev_header = free_list_;
        alc_header *header = prev_header->next;

        while (header != end_header) {
            MADI_ASSERT(header != NULL);
            MADI_ASSERT(header->next != NULL);

            if (h < header)
                break;

            prev_header = header;
            header = header->next;
        }

        prev_header->next = h;
        h->next = header;

        alc_header *new_header = try_merge(prev_header, h);
        try_merge(new_header, header);
    }
#endif
}
}
