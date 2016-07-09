#ifndef MADI_UNI_WORKER_INL_H
#define MADI_UNI_WORKER_INL_H

#include "worker.h"
#include "../uth_config.h"
#include "../madi-inl.h"
#include "taskq-inl.h"
#include "context.h"
#include "../debug.h"
#include "../madi.h"
#include "uth.h"

namespace madi {

    /*
      - スレッドの状態
        - running
        - lightweight-suspended (thread in taskq): thread = { sp }
        - suspended (packed, thread not in taskq): packed_thread = { sp, stack }

      - 設計
        - lightweight-suspended と suspended の違いは、worker 全体の
          コールスタック上にデータがあるか、ヒープなどの領域にコールスタックの
          一部が退避されているか
        - sp から bottom_ptr の間のスタックの状態については、
          lightweight-suspended と suspended の両方について同じとする

      - タスク生成時の lightweight suspend/resume
        - caller-/callee-saved registers を call stack に push
        - RESUMEラベルを push
        - sp を取得

        - taskq に sp を push
        - ユーザ関数呼び出し (テンプレートで inline 展開)
        - taskq から sp を pop
        - pop できなかったら scheduler コードに移行
        - (pop できたらタスク生成関数から return,
           saved registers はまとめて破棄される)
          
        - RESUMEラベル:
        - saved registers を pop
        - (タスク生成関数から return)
      
      - lightweight-suspended threads の pack (at steal)
        - (taskq から thread を steal)
        - bottom_ptr から sp までをヒープにコピー
        - bottom_ptr = sp;

      - resume (タスクを実行していない場合, スケジューラ内)
        - packed stack を worker のコールスタックに書き戻す (iso-address)
          (タスクを実行している状態で resume を実行した場合、
          現在実行しているタスクを壊さずにスタックを書き戻すことができるか？)
        - sp を設定
        - ret 命令により、スタック上に保存した PC (RESUMEラベル) にジャンプ

      - resume (タスクが実行中の場合)
        - resume するタスクのスタック使用範囲を計算
        - worker のコールスタックについて、上記範囲をヒープに退避する
        - resume するタスクの packed stack を worker のコールスタック上に書き戻す
        - sp を設定
        - ret 命令により、スタック上に保存した PC (RESUMEラベル) にジャンプ

      - suspend (at suspend)
        - caller-/callee-saved registers を call stack に push
        - RESUMEラベルを call stack に push
        - sp を取得

        - call stack をヒープにコピー
        - 関数呼び出し k(sp)
        - (NOT REACHED)

        - RESUMEラベル:
        - saved registers を pop
        - (suspend 関数から return)
    */
    template <class F, class... Args>
    void worker_do_fork(context *ctx_ptr, void *f_ptr, void *arg_ptr)
    {
        context& ctx = *ctx_ptr;
        F f = *(F *)f_ptr;
        std::tuple<Args...> arg = *(std::tuple<Args...> *)arg_ptr;

        MADI_CONTEXT_PRINT(3, &ctx);
        MADI_CONTEXT_ASSERT(&ctx);

        worker& w0 = madi::current_worker();

        MADI_DPUTS3("parent_ctx = %p", ctx.parent);

        if (w0.is_main_task_) {
            MADI_DPUTS3("register context %p to main_ctx_", &ctx);

            // the main thread must not be pushed into taskq
            // because the main thread must not be stolen by another 
            // process.
            w0.main_ctx_ = &ctx;
            w0.is_main_task_ = false;

            MADI_DPUTS2("main task saved [%p, %p) (size = %zu)",
                        ctx.stack_ptr(),
                        ctx.stack_ptr() + ctx.stack_size(),
                        ctx.stack_size());
        } else {
            MADI_DPUTS3("push context %p", &ctx);

            // push the current (parent) thread
            taskq_entry entry;
            entry.frame_base = ctx.top_ptr();
            entry.frame_size = ctx.stack_size();
            entry.ctx = &ctx;

            MADI_TENTRY_ASSERT(&entry);

            MADI_CHECK(entry.frame_size < 128 * 1024);

            w0.taskq_->push(entry);
        }

        MADI_UTH_COMM_POLL();

        // calculate stack usage for profiling
        {
            uint8_t *stack_top;
            MADI_GET_CURRENT_STACK_TOP(&stack_top);

            size_t stack_usage = w0.stack_bottom_ - stack_top;
            w0.max_stack_usage_ = std::max(w0.max_stack_usage_, stack_usage);
        }

        w0.parent_ctx_ = &ctx;

        // execute a child thread
        tuple_apply<void>::f<F, Args...>(f, arg);

        // renew worker (the child thread may be stolen)
        worker& w1 = madi::current_worker();

        // pop the parent thread
        taskq_entry *entry = w1.taskq_->pop();
 
        if (entry != NULL) {
            // the entry is not stolen

            MADI_DPUTS3("pop context %p", &ctx);

            MADI_CONTEXT_ASSERT(&ctx);
            MADI_CONTEXT_ASSERT_WITHOUT_PARENT(ctx.parent);

            MADI_TENTRY_ASSERT(entry);
            MADI_ASSERT(entry->frame_base == ctx.top_ptr());
            MADI_ASSERT(entry->frame_size == ctx.stack_size());
            MADI_ASSERT(entry->ctx == &ctx);

            w1.parent_ctx_ = ctx.parent;
        } else {
            // the parent thread is main or stolen
            // (assume that a parent thread resides in the deeper
            // position on a task deque than its child threads.)

            MADI_CONTEXT_ASSERT_WITHOUT_PARENT(&ctx);

            w1.go();

            MADI_NOT_REACHED;
        }
    }

    template <class F, class... Args>
    void worker::fork(F f, Args... args)
    {
        worker& w0 = *this;

        context *prev_ctx = w0.parent_ctx_;

        MADI_CHECK((uintptr_t)prev_ctx >= 128 * 1024);

        MADI_DPUTS3("&prev_ctx = %p", &prev_ctx);

        MADI_CONTEXT_PRINT(3, prev_ctx);
        MADI_CONTEXT_ASSERT_WITHOUT_PARENT(prev_ctx);

        void (*fp)(context *, void *, void *) = worker_do_fork<F, Args...>;

        std::tuple<Args...> arg(args...);

#if MADI_ARCH_TYPE == MADI_ARCH_SPARC64
        uint8_t *sp0, *fp0, *i70;
        MADI_GET_CURRENT_SP(&sp0);
        MADI_GET_CURRENT_FP(&fp0);
        MADI_GET_CURRENT_I7(&i70);
        MADI_DPUTS3("sp = %p", sp0);
        MADI_DPUTS3("fp = %p", fp0);
        MADI_DPUTS3("i7 = %p", i70);
#endif
        // save the current context to the stack
        // (copy register values to the current stack)
        MADI_SAVE_CONTEXT_WITH_CALL(prev_ctx, fp, (void *)&f, (void *)&arg);

#if MADI_ARCH_TYPE == MADI_ARCH_SPARC64
        uint8_t *sp1, *fp1, *i71;
        MADI_GET_CURRENT_SP(&sp1);
        MADI_GET_CURRENT_FP(&fp1);
        MADI_GET_CURRENT_I7(&i71);
        MADI_DPUTS3("sp = %p", sp1);
        MADI_DPUTS3("fp = %p", fp1);
        MADI_DPUTS3("i7 = %p", i71);
#endif
        MADI_DPUTS3("resumed: parent_ctx = %p", prev_ctx);
        MADI_DPUTS3("&prev_ctx = %p", &prev_ctx);

        worker& w1 = madi::current_worker();
           
        MADI_DPUTS3("&madi_process = %p", &madi_process);
        MADI_DPUTS3("worker_id = %zu", madi_worker_id);

        MADI_CONTEXT_PRINT(3, prev_ctx);
        MADI_CONTEXT_ASSERT_WITHOUT_PARENT(prev_ctx);

        w1.parent_ctx_ = prev_ctx;

        long t0 = g_prof->current_steal().tmp;
        if (t0 != 0) {
            long t1 = rdtsc();
            g_prof->current_steal().resume = t1 - t0;
            g_prof->next_steal();

            MADI_DPUTSR1("resume done");
        }
    }

    void resume_saved_context(saved_context *sctx, saved_context *next_sctx);

    inline void worker::go()
    {
//        MADI_ASSERT(main_ctx_ != NULL);

        if (!is_main_task_ && main_ctx_ != NULL) {
            // this task is not the main task,
            // and the main task is not saved (packed).
            is_main_task_ = true;

            MADI_DPUTS1("a main task is resuming");

            MADI_CONTEXT_PRINT(1, main_ctx_);
            MADI_RESUME_CONTEXT(main_ctx_);
        } else if (!waitq_.empty()) {
            // here, the main task is in the waiting queue

            saved_context *sctx = waitq_.front();
            waitq_.pop_front();

            MADI_DPUTSB1("resuming a waiting task");

            resume_saved_context(NULL, sctx);
        } else {
            MADI_NOT_REACHED;
        }
        
        MADI_NOT_REACHED;
    }

    inline void worker::exit()
    {
        MADI_NOT_REACHED;
    }

/*
 * this macro must be called in the same function the ctx saved.
 */
#define MADI_THREAD_PACK(ctx_ptr, is_main, sctx_ptr)                    \
    do {                                                                \
        context& ctx__ = *(ctx_ptr);                                    \
        void *ip__ = ctx__.instr_ptr();                                 \
        void *sp__ = ctx__.stack_ptr();                                 \
        uint8_t *top__ = ctx__.top_ptr();                               \
                                                                        \
        /*size_t stack_size__ = ctx.stack_size();*/                     \
        size_t stack_size__ = ctx__.stack_size();                       \
        size_t size__ = offsetof(saved_context, partial_stack) + stack_size__;\
                                                                        \
        /* copy stack frames to heap */                                 \
        saved_context *sctx__ = (saved_context *)malloc(size__);        \
        sctx__->is_main_task = (is_main);                               \
        sctx__->ip = ip__;                                              \
        sctx__->sp = sp__;                                              \
        sctx__->ctx = &ctx__;                                           \
        sctx__->stack_top = top__;                                      \
        sctx__->stack_size = stack_size__;                              \
        memcpy(sctx__->partial_stack, top__, stack_size__);             \
                                                                        \
        MADI_DPUTSB2("suspended [%p, %p) (size = %zu)",                 \
                     top__, top__ + stack_size__, stack_size__);        \
                                                                        \
        *(sctx_ptr) = sctx__;                                           \
    } while (false)

    
    template <class F, class... Args>
    void worker_do_suspend(context *ctx_ptr, void *f_ptr, void *arg_ptr)
    {
        F f = (F)f_ptr;
        std::tuple<saved_context *, Args...> arg =
            *(std::tuple<saved_context *, Args...> *)arg_ptr;
        
        worker& w0 = madi::current_worker();

        // pack the current thread (stack)
        saved_context *sctx = NULL;
        MADI_THREAD_PACK(ctx_ptr, w0.is_main_task_, &sctx);

        if (!w0.is_main_task_) {
            MADI_CONTEXT_ASSERT(ctx_ptr);
            MADI_SCONTEXT_ASSERT(sctx);
        }

        std::get<0>(arg) = sctx;

        w0.parent_ctx_ = ctx_ptr;

        // execute a thread start function
        tuple_apply<void>::f(f, arg);

        MADI_NOT_REACHED;
    }
 
    template <class F, class... Args>
    void worker::suspend(F f, Args... args)
    {
        // suspend the current thread, and create a new thread which
        // executes f(packed_thread, args...).

        worker& w0 = *this;

        context *prev_ctx = w0.parent_ctx_;

        if (!w0.is_main_task_)
            MADI_CONTEXT_ASSERT_WITHOUT_PARENT(prev_ctx);

        void (*fp)(context *, void *, void *) = worker_do_suspend<F, Args...>;

        saved_context *sctx = NULL;
        std::tuple<saved_context *, Args...> arg(sctx, args...);

        // save current state
        MADI_SAVE_CONTEXT_WITH_CALL(prev_ctx, fp, (void *)f, (void *)&arg);

        if (!w0.is_main_task_)
            MADI_CONTEXT_ASSERT_WITHOUT_PARENT(prev_ctx);

        madi::current_worker().parent_ctx_ = prev_ctx;
    }

}

#endif
