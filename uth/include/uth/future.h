#ifndef MADI_FUTURE_H
#define MADI_FUTURE_H

#include "madi.h"
#include "misc.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace madi {

    class uth_comm;

    class dist_spinlock {
        uth_comm& c_;
        int **locks_;
    public:
        dist_spinlock(uth_comm& c);
        ~dist_spinlock();

        bool trylock(madi::pid_t target);
        void lock(madi::pid_t target);
        void unlock(madi::pid_t unlock);
    };

    template <class T>
    class dist_pool {
        uth_comm& c_;
        int size_;
        dist_spinlock locks_;
        int **idxes_;
        T **data_;
    public:
        dist_pool(uth_comm& c, int size);
        ~dist_pool();

        bool empty(madi::pid_t target);

        bool push_remote(T& v, madi::pid_t target);

        void begin_pop_local();
        void end_pop_local();
        bool pop_local(T *buf);
    };

    class future_pool {
        MADI_NONCOPYABLE(future_pool);

        enum constants {
            MAX_ENTRY_BITS = 16,
        };

        template <class T>
        struct entry {
            T value;
            int done;
        };

        int ptr_;
        int buf_size_;
        uint8_t **remote_bufs_;

        std::vector<int> id_pools_[MAX_ENTRY_BITS];

        struct retpool_entry {
            int id;
            int size;
        };

        dist_pool<retpool_entry> *retpools_;
    public:
        future_pool();
        ~future_pool();

        void initialize(uth_comm& c, size_t n_entries);
        void finalize(uth_comm& c);

        template <class T>
        int get();

        template <class T>
        void fill(int id, madi::pid_t pid, T& value);

        template <class T>
        bool synchronize(int id, madi::pid_t pid, T *value);

    private:
        template <class T>
        void reset(int id);

        void move_back_returned_ids();
    };
}

namespace madm {

    template <class T>
    class future {

        // shared object
        int future_id_;
        madi::pid_t pid_;

        template <class F, class... Args>
        static void start(int future_id, madi::pid_t pid, F f, Args... args);
       
    public:
        future();

        template <class F, class... Args>
        future(F f, Args... args);

        template <class F, class... Args>
        void spawn(F f, Args... args);

        T touch();
    };
    
}

#endif
