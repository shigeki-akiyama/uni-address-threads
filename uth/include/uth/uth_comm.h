#ifndef MADI_UTH_COMM_H
#define MADI_UTH_COMM_H

#include "uth_config.h"
#include "madi.h"
#include "debug.h"
#include <cstddef>
#include <cstring>

#define MADI_USE_MCOMM 1

#include <madm/madm_comm_config.h>

#if !MADI_USE_MCOMM

#if   MADI_COMM_LAYER == MADI_COMM_LAYER_SHMEM
#include "shmem.h"
#elif MADI_COMM_LAYER == MADI_COMM_LAYER_FX10
#include "fjrdma.h"
#else
#error ""
#endif

#endif

namespace madi {

    class collectives;

    class uth_comm {
        MADI_NONCOPYABLE(uth_comm);

#if MADI_USE_MCOMM
        class rdma_type { public: rdma_type(uth_comm&) {} };
#else

#if   MADI_COMM_LAYER == MADI_COMM_LAYER_SHMEM
        typedef shmem rdma_type;
#elif MADI_COMM_LAYER == MADI_COMM_LAYER_FX10
        typedef fjrdma rdma_type;
#else
#error ""
#endif

#endif

        int rdma_id_;
        rdma_type rdma_;

        collectives *coll_;

        size_t buffer_size_;
        uint8_t *buffer_;

        bool initialized_;

    public:
       
        uth_comm() : rdma_id_(-1), rdma_(*this), coll_(NULL),
                     buffer_size_(0), buffer_(NULL),
                     initialized_(false) {}
        ~uth_comm() {}

        bool initialize(int& argc, char**& argv);
        void finalize();

        template <class... Args>
        void start(void (*f)(int argc, char **argv, Args... args),
                   int argc, char **argv, Args... args);
        void exit(int exitcode) MADI_NORETURN;
        
        bool initialized();

        madi::pid_t get_pid();
        size_t get_n_procs();
            
        void **malloc_shared(size_t size);
        void free_shared(void **p);
        void *malloc_shared_local(size_t size);
        void free_shared_local(void *p);

        void put(void *dst, void *src, size_t size, madi::pid_t target);
        void get(void *dst, void *src, size_t size, madi::pid_t target);
        void put_buffered(void *dst, void *src, size_t size,
                          madi::pid_t target);
        void get_buffered(void *dst, void *src, size_t size,
                          madi::pid_t target);
        void put_value(int *dst, int value, madi::pid_t target);
        void put_value(long *dst, long value, madi::pid_t target);
        int  get_value(int *src, madi::pid_t target);
        long get_value(long *src, madi::pid_t target);
        void swap(int *dst, int *src, madi::pid_t target);
        int fetch_and_add(int *dst, int value, madi::pid_t target);

        void **reg_mmap_shared(void *addr, size_t size);
        void reg_munmap_shared(void **ptrs, void *addr, size_t size);

        void reg_put(void *dst, void *src, size_t size, madi::pid_t target);
        void reg_get(void *dst, void *src, size_t size, madi::pid_t target);

        void barrier();
        bool barrier_try();

        template <class T>
        void broadcast(T& dst, const T& src, size_t root);

        void poll();
    };

#if MADI_NEED_POLL
#define MADI_UTH_COMM_POLL() madi::proc().com().poll()
#else
#define MADI_UTH_COMM_POLL() ((void)0)
#endif

    inline void uth_comm::put_buffered(void *dst, void *src, size_t size,
                                       madi::pid_t target)
    {
        MADI_ASSERT(size <= buffer_size_);

        memcpy(buffer_, src, size);

        put(dst, buffer_, size, target);
    }

    inline void uth_comm::get_buffered(void *dst, void *src, size_t size,
                                       madi::pid_t target)
    {
        MADI_ASSERT(size <= buffer_size_);

        get(buffer_, src, size, target);

        memcpy(dst, buffer_, size);
    }

}

#endif
