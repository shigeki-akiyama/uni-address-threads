#ifndef MADI_ISO_STACK_WORKER_INL_H
#define MADI_ISO_STACK_WORKER_INL_H

#include "worker.h"
#include "context.h"
#include "debug.h"

#include "taskq-inl.h"
#include "stack_pool-inl.h"


namespace madi {

    inline context worker::current_ctx() { return current_ctx_; }
    inline context worker::main_ctx() { return main_ctx_; }

    template <class F, class... Args>
    void worker::switch_to(context ctx, F callback, Args... args)
    {
        context prev_ctx = current_ctx_;

        current_ctx_ = ctx;

        prev_ctx.switch_to(ctx, callback, args...);
    }

    template <class F, class... Args>
    inline void worker::fork_callback(context old_ctx, context new_ctx,
                                         worker *self,
                                         F f, Args... args)
    {
        taskq_entry entry(old_ctx);
        self->taskq_.push(entry);

        f(args...);

        self->exit();
    }

    template <class F, class... Args>
    inline void worker::fork(F f, Args... args)
    {
        size_t stack_size = 8192; // FIXME
        context new_ctx(stack_size, salloc_);
        
        switch_to(new_ctx, fork_callback<F, Args...>, 
                  new_ctx, this, f, args...);
    }

    inline void worker::exit_callback(context old_ctx, worker *self)
    {
        old_ctx.destroy(self->salloc_);
    }

    inline void worker::exit()
    {
        go(exit_callback);

        MADI_NOT_REACHED;
    }

    inline void worker::do_scheduler_work()
    {
        MADI_UNDEFINED;
        /*
        packed_thread th = steal();
        thread::resume(th);
        */
    }

    inline void worker::go()
    {
        MADI_UNDEFINED;
    }

    template <class F>
    inline void worker::go(F f)
    {
        context next_ctx;

        // execute a task in taskq if it exists
        taskq_entry entry;
        bool success = taskq_.pop(&entry);

        if (success) {
            next_ctx = entry.ctx_;
        } else {
            // switch to the main thread
            next_ctx = main_ctx_;
        }

        switch_to(next_ctx, f, this);

        MADI_NOT_REACHED;
    }

}

#endif
