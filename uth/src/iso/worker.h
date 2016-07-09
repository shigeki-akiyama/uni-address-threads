#ifndef MADI_ISO_STACK_WORKER_H
#define MADI_ISO_STACK_WORKER_H

#include "context.h"
#include "taskq.h"
#include "stack_pool.h"
#include "debug.h"

namespace madi {

    class worker {
//        MADI_NONCOPYABLE(worker);

        taskque taskq_;

        context current_ctx_;
        context main_ctx_;

        bool done_;
        bool main_waiting_;

        stack_allocator salloc_;

        void do_scheduler_work();
        
    public:
        worker();
        ~worker();

        worker(const worker& w);  // for std::vector

        void initialize();
        void finalize();

        context current_ctx();
        context main_ctx();

        void start(void (*f)(int, char **), int argc, char **argv);
        //        template <class F, class... Args>
        //        void start(F f, Args... args);

        void notify_done();

        template <class F, class... Args>
        void fork(F f, Args... args);

        void exit();

        context steal();

        void go();

        template <class F>
        void go(F callback);
    private:
        template <class F, class... Args>
        void switch_to(context ctx, F callback, Args... args);

        void switch_to_main();

        static void start_callback(context old_ctx, context new_ctx,
                                   worker *self,
                                   void (*f)(int, char **), 
                                   int argc, char **argv);

        template <class F, class... Args>
        static void fork_callback(context old_ctx, context new_ctx,
                                  worker *self,
                                  F f, Args... args);

        static void exit_callback(context old_ctx, worker *self);
     };
    
}

#endif
