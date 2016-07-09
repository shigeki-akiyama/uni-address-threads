#ifndef MADI_FUTURE_INL_H
#define MADI_FUTURE_INL_H

#include "future.h"
#include "uth_comm-inl.h"
#include "uni/worker-inl.h"
#include <madm/threadsafe.h>

namespace madi {

    inline dist_spinlock::dist_spinlock(uth_comm& c) :
        c_(c),
        locks_(NULL)
    {
        madi::pid_t me = c.get_pid();

        locks_ = (int **)c_.malloc_shared(sizeof(int));
        *locks_[me] = 0;
    }

    inline dist_spinlock::~dist_spinlock()
    {
        c_.free_shared((void **)locks_);
    }

    inline bool dist_spinlock::trylock(madi::pid_t target)
    {
        int *lock = locks_[target];
        int v = c_.fetch_and_add(lock, 1, target);
        return (v == 0);
    }

    inline void dist_spinlock::lock(madi::pid_t target)
    {
        while (!trylock(target))
            MADI_UTH_COMM_POLL();
    }

    inline void dist_spinlock::unlock(madi::pid_t target)
    {
        int *lock = locks_[target];
        c_.put_value(lock, 0, target);
    }

    template <class T>
    dist_pool<T>::dist_pool(uth_comm& c, int size) :
        c_(c),
        size_(size),
        locks_(c),
        idxes_(NULL),
        data_(NULL)
    {
        madi::pid_t me = c.get_pid();

        idxes_ = (int **)c_.malloc_shared(sizeof(int));
        data_ = (T **)c_.malloc_shared(sizeof(T) * size);

        *idxes_[me] = 0;
    }

    template <class T>
    dist_pool<T>::~dist_pool()
    {
        c_.free_shared((void **)idxes_);
        c_.free_shared((void **)data_);
    }

    template <class T>
    bool dist_pool<T>::empty(madi::pid_t target)
    {
        madi::pid_t me = c_.get_pid();

        int *idx_ptr = idxes_[target];

        int idx;
        if (target == me) {
            idx = *idx_ptr;
        } else {
            idx = c_.get_value(idx_ptr, target);
        }

        MADI_ASSERT(0 <= idx && idx < size_);

        return idx == 0;
    }

    template <class T>
    bool dist_pool<T>::push_remote(T& v, madi::pid_t target)
    {
        locks_.lock(target);

        int *idx_ptr = idxes_[target];
        int idx = c_.fetch_and_add(idx_ptr, 1, target);

        MADI_ASSERT(0 <= idx && idx < size_);

        bool success;
        if (idx < size_) {
            T *buf = data_[target] + idx;

            // v is on a stack registered for RDMA
            c_.put_buffered(buf, &v, sizeof(T), target);

            success = true;
        } else {
            c_.put_value(idx_ptr, idx, target);
            success = false;
        }

        locks_.unlock(target);
        return success;
    }

    template <class T>
    void dist_pool<T>::begin_pop_local()
    {
        madi::pid_t target = c_.get_pid();
        locks_.lock(target);
    }

    template <class T>
    void dist_pool<T>::end_pop_local()
    {
        madi::pid_t target = c_.get_pid();
        locks_.unlock(target);
    }

    template <class T>
    bool dist_pool<T>::pop_local(T *buf)
    {
        madi::pid_t target = c_.get_pid();

        int current_idx = *idxes_[target];

        MADI_ASSERT(0 <= current_idx && current_idx < size_);

        if (current_idx == 0)
            return false;

        int idx = current_idx - 1;
        T *src = data_[target] + idx;
        memcpy(buf, src, sizeof(T));

        *idxes_[target] -= 1;

        return true;
    }

    inline size_t index_of_size(size_t size)
    {
        return 64UL - static_cast<size_t>(__builtin_clzl(size - 1));
    }

    inline future_pool::future_pool() :
        ptr_(0), buf_size_(0), remote_bufs_(NULL), retpools_(NULL)
    {
    }
    inline future_pool::~future_pool()
    {
    }

    inline void future_pool::initialize(uth_comm& c, size_t buf_size)
    {
        int retpool_size = 16 * 1024;

        ptr_ = 0;
        buf_size_ = (int)buf_size;

        remote_bufs_ = (uint8_t **)c.malloc_shared(buf_size);
        retpools_ = new dist_pool<retpool_entry>(c, retpool_size);
    }

    inline void future_pool::finalize(uth_comm& c)
    {
        c.free_shared((void **)remote_bufs_);

        for (size_t i = 0; i < MAX_ENTRY_BITS; i++)
            id_pools_[i].clear();

        delete retpools_;

        ptr_ = 0;
        buf_size_ = 0;
        remote_bufs_ = NULL;
        retpools_ = NULL;
    }

    template <class T>
    void future_pool::reset(int id)
    {
        uth_comm& c = madi::proc().com();
        madi::pid_t me = c.get_pid();

        entry<T> *e = (entry<T> *)(remote_bufs_[me] + id);

        e->done = 0;
    }

    inline void future_pool::move_back_returned_ids()
    {
        retpools_->begin_pop_local();

        size_t count = 0;
        retpool_entry entry;
        while (retpools_->pop_local(&entry)) {
            size_t idx = index_of_size(entry.size);
            id_pools_[idx].push_back(entry.id);

            count += 1;
        }

        retpools_->end_pop_local();

        MADI_DPUTSB1("move back returned future ids: %zu", count);
    }

    template <class T>
    int future_pool::get()
    {
        madi::pid_t me = madi::proc().com().get_pid();

        size_t entry_size = sizeof(entry<T>);
        size_t idx = index_of_size(entry_size);

        int real_size = 1 << idx;

        // move future ids from the return pool to the local pool
        if (!retpools_->empty(me)) {
            move_back_returned_ids();
        }

        // pop a future id from the local pool
        if (!id_pools_[idx].empty()) {
            int id = id_pools_[idx].back();
            id_pools_[idx].pop_back();

            reset<T>(id);
            return id;
        }

        // if pool is empty, allocate a future id from ptr_
        if (ptr_ + real_size < buf_size_) {
            int id = ptr_;
            ptr_ += real_size;
            return id;
        }

        madi::die("future pool overflow");
    }

    template <class T>
    void future_pool::fill(int id, madi::pid_t pid, T& value)
    {
        uth_comm& c = madi::proc().com();
        madi::pid_t me = c.get_pid();

        entry<T> *e = (entry<T> *)(remote_bufs_[pid] + id);

        if (pid == me) {
            e->value = value;
            comm::threadsafe::wbarrier();
            e->done = 1;
        } else {
            // value is on a stack registered for RDMA
            c.put_buffered(&e->value, &value, sizeof(value), pid);
            c.put_value(&e->done, 1, pid);
        }
    }

    template <class T>
    bool future_pool::synchronize(int id, madi::pid_t pid, T *value)
    {
        uth_comm& c = madi::proc().com();
        madi::pid_t me = c.get_pid();

        entry<T> *e = (entry<T> *)(remote_bufs_[pid] + id);

        entry<T> entry_buf;
        if (pid != me) {
#if 0
            // Is this safe in terms of memory ordering???
            c.get(&entry_buf, e, sizeof(e), pid);
#else
            entry_buf.done = c.get_value(&e->done, pid);

            if (entry_buf.done == 1) {
                // entry_buf is on a stack registered for RDMA
                c.get_buffered(&entry_buf.value, &e->value,
                               sizeof(e->value), pid);
            }
#endif
            e = &entry_buf;
        }

        MADI_ASSERT(0 <= id && id < buf_size_);

        if (e->done) {
            if (pid == me) {
                size_t idx = index_of_size(sizeof(entry<T>));
                id_pools_[idx].push_back(id);
            } else {
                // return fork-join descriptor to processor pid.
                retpool_entry rpentry = { id, (int)sizeof(entry<T>) };
                bool success = retpools_->push_remote(rpentry, pid);

                if (success) {
                    MADI_DPUTSR1("push future %d to return pool(%zu)",
                                id, pid);
                } else {
                    madi::die("future return pool becomes full");
                }
            }
 
            comm::threadsafe::rbarrier();
            *value = e->value;
        }

        return e->done;
    }
}

namespace madm {

    template <class T>
    template <class F, class... Args>
    inline void future<T>::start(int future_id, madi::pid_t pid, 
                                 F f, Args... args)
    {
        T value = f(args...);

        madi::worker *w = &madi::current_worker();
        w->fpool().fill(future_id, pid, value);
    }

    template <class T>
    future<T>::future() : future_id_(0), pid_(0) {}

    template <class T>
    template <class F, class... Args>
    future<T>::future(F f, Args... args) :
        future_id_(0), pid_(0)
    {
        spawn(f, args...);
    }

    template <class T>
    template <class F, class... Args>
    void future<T>::spawn(F f, Args... args)
    {
        madi::worker& w = madi::current_worker();
        madi::uth_comm& c = madi::proc().com();

        future_id_ = w.fpool().get<T>();
        pid_ = c.get_pid();

        w.fork(start<F, Args...>, future_id_, pid_, f, args...);
//        w.fork(start<T (Args...), Args...>, future_id_, pid_, f, args...);
    }

    template <class T>
    T future<T>::touch()
    {
        T value;

        for (;;) {
            madi::worker& w = madi::current_worker();
            madi::future_pool& pool = w.fpool();

            if (pool.synchronize(future_id_, pid_, &value))
                break;

            w.do_scheduler_work();
        }

        return value;
    }

 }

#endif
