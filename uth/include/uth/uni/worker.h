#ifndef MADI_ISO_STACK_EX_WORKER_H
#define MADI_ISO_STACK_EX_WORKER_H

#include "../madi.h"
#include "taskq.h"
#include "context.h"
#include "../future.h"
#include "../debug.h"
#include <deque>
#include <tuple>

namespace madi {

    struct start_params;

    class worker {
        template <class F, class... Args>
        friend void worker_do_fork(context *ctx_ptr,
                                   void *f_ptr, void *arg_ptr);

        template <class F, class... Args>
        friend void worker_do_suspend(context *ctx_ptr,
                                      void *f_ptr, void *arg_ptr);

        friend void worker_start(void *arg0, void *arg1, void *arg2,
                                 void *arg3);
        friend void worker_do_start(context *ctx, void *arg0, void *arg1);

        friend void madi_worker_do_resume_remote(void *p0, void *p1, 
                                                 void *p2, void *p3);
        friend void resume_saved_context(saved_context *sctx, 
                                         saved_context *next_sctx);
        friend void resume_remote_context(saved_context *sctx, 
                                          std::tuple<taskq_entry *,
                                          madi::pid_t, taskque *, tsc_t> *arg);

        context *parent_ctx_;
        bool is_main_task_;
        size_t max_stack_usage_;
        uint8_t *stack_bottom_;

        taskque *taskq_;
        taskque **taskq_array_;
        taskq_entry **taskq_entries_array_;
        taskque *taskq_buf_;
        taskq_entry *taskq_entry_buf_;

        future_pool fpool_;

        context *main_ctx_;
        std::deque<saved_context *> waitq_;
        
        bool done_;

    public:
        worker();
        ~worker();

        worker(const worker& sched);
        const worker& operator=(const worker& sched);

        void initialize(uth_comm& c);
        void finalize(uth_comm& c);

        void start(void (*f)(int, char **), int argc, char **argv);
        //        template <class F, class... Args>
        //        void start(F f, Args... args);

        void notify_done();

        template <class F, class... Args>
        void fork(F f, Args... args);

        void exit();

        template <class F, class... Args>
        void suspend(F f, Args... args);

        void do_scheduler_work();

        future_pool& fpool() { return fpool_; }
        taskque& taskq() { return *taskq_; }
        std::deque<saved_context *>& waitq() { return waitq_; }

        size_t max_stack_usage() const { return max_stack_usage_; }

    private:
        void go();
        static void do_resume(worker& w, const taskq_entry& entry,
                              madi::pid_t victim);
        bool steal_with_lock(taskq_entry *entry, madi::pid_t *victim,
                             taskque **taskq);
        bool is_main_task();
    };
    
}

#endif
