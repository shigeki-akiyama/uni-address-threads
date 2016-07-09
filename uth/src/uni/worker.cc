#include "uni/worker.h"
#include "madi.h"
#include "uni/taskq.h"
#include "uth_options.h"
#include "debug.h"

#include "madi-inl.h"
#include "process-inl.h"
#include "iso_space-inl.h"
#include "uth_comm-inl.h"
#include "future-inl.h"
#include "uni/worker-inl.h"

#include <unistd.h>


using namespace madi;

extern "C" {

void madi_worker_do_resume_saved_context(void *p0, void *p1, void *p2,
                                         void *p3);
void madi_worker_do_resume_remote_context(void *p0, void *p1, void *p2,
                                          void *p3);

}

namespace madi {

worker::worker() :
    parent_ctx_(NULL),
    is_main_task_(false),
    taskq_(NULL), taskq_array_(NULL), taskq_entries_array_(NULL),
    fpool_(),
    main_ctx_(NULL),
    waitq_(),
    done_(false)
{
}

worker::~worker()
{
}

worker::worker(const worker& sched) :
    parent_ctx_(NULL),
    is_main_task_(false),
    taskq_(), taskq_array_(NULL), taskq_entries_array_(NULL),
    fpool_(),
    main_ctx_(NULL), 
    waitq_(),
    done_(false)
{
}

const worker& worker::operator=(const worker& sched)
{
    MADI_NOT_REACHED;
}

void worker::initialize(uth_comm& c)
{
    pid_t me = c.get_pid();

    size_t n_entries = madi::uth_options.taskq_capacity;
    size_t entries_size = sizeof(taskq_entry) * n_entries;

    taskque ** taskq_array =
        (taskque **)c.malloc_shared(sizeof(taskque));

    MADI_ASSERT(taskq_array[me] != NULL);

    taskq_entry ** taskq_entries_array =
        (taskq_entry **)c.malloc_shared(entries_size);

    MADI_ASSERT(taskq_entries_array[me] != NULL);

    taskque *taskq_buf =
        (taskque *)c.malloc_shared_local(sizeof(taskque));

    MADI_ASSERT(taskq_buf != NULL);

    taskq_entry *taskq_entry_buf =
        (taskq_entry *)c.malloc_shared_local(sizeof(taskq_entry));

    MADI_ASSERT(taskq_entry_buf != NULL);

    taskque *taskq = new (taskq_array[me]) taskque();
    taskq->initialize(c, taskq_entries_array[me], n_entries);

    taskq_ = taskq;
    taskq_array_ = taskq_array;
    taskq_entries_array_ = taskq_entries_array;
    taskq_buf_ = taskq_buf;
    taskq_entry_buf_ = taskq_entry_buf;

    MADI_ASSERT(waitq_.size() == 0);
    waitq_.clear();

    size_t future_buf_size = 16 * 1024; //128 * 1024;
    fpool_.initialize(c, future_buf_size);
}

void worker::finalize(uth_comm& c)
{
    fpool_.finalize(c);
    taskq_->finalize(c);

    c.free_shared((void **)taskq_array_);
    c.free_shared((void **)taskq_entries_array_);
    c.free_shared_local((void *)taskq_buf_);
    c.free_shared_local((void *)taskq_entry_buf_);

    taskq_ = NULL;
    taskq_array_ = NULL;
    taskq_entries_array_ = NULL;
    taskq_buf_ = NULL;
    taskq_entry_buf_ = NULL;
}

struct start_params {
    worker *w;
    void (*init_f)(int, char **);
    int argc;
    char **argv;
    uint8_t *stack;
    size_t stack_size;
};

void worker_do_start(context *ctx, void *arg0, void *arg1)
{
    start_params *p = (start_params *)arg0;

    MADI_CONTEXT_PRINT(2, ctx);

    worker& w = madi::current_worker();

    uint8_t *stack_top;
    MADI_GET_CURRENT_STACK_TOP(&stack_top);

    w.max_stack_usage_ = 0;
    w.stack_bottom_ = stack_top;
    w.parent_ctx_ = ctx;
    w.is_main_task_ = true;

    // execute the start function
    p->init_f(p->argc, p->argv);

    MADI_DPUTS2("user code in a main thread is finished");

    madi::barrier();

    w.parent_ctx_ = NULL;
    w.is_main_task_ = false;
}

__attribute__((noinline)) void make_it_non_leaf() {}

void worker_start(void *arg0, void *arg1, void *arg2, void *arg3)
{
    start_params *params = (start_params *)arg0;
    /* this function call makes worker_start
       look like a non-leaf function.
       this is necessary because otherwise, 
       the compiler may generate code that does
       not properly align stack pointer before
       the call instruction done by 
       MADI_SAVE_CONTEXT_WITH_CALL. */
    make_it_non_leaf();

    MADI_DPUTS2("worker start");

    MADI_SAVE_CONTEXT_WITH_CALL(NULL, worker_do_start, params, NULL);

    MADI_DPUTS2("worker end");
}

void worker::start(void (*f)(int, char **), int argc, char **argv)
{
    // allocate an iso-address stack
    iso_space& ispace = madi::proc().ispace();
    uint8_t *stack = (uint8_t *)ispace.stack();
    size_t stack_size = ispace.stack_size();

    madi::pid_t me = madi::proc().com().get_pid();

    MADI_DPUTS1("pid            = %zu", me);
    MADI_DPUTS1("stack_region   = [%p, %p)", stack, stack + stack_size);
    MADI_DPUTS1("stack_size     = %zu", stack_size);

    start_params params = { this, f, argc, argv, stack, stack_size };

#if 0
    // FIXME: safe implementation
    if (me == 0) {
        // switch from the default OS thread stack to the iso-address stack,
        // and execute scheduler function
        MADI_CALL_ON_NEW_STACK(stack, stack_size,
                               worker_start, &params, NULL, NULL, NULL);
    } else {
        worker_start(&params, NULL, NULL, NULL);
    }
#else
    // switch from the default OS thread stack to the iso-address stack,
    // and execute scheduler function
    MADI_CALL_ON_NEW_STACK(stack, stack_size,
                           worker_start, &params, NULL, NULL, NULL);
#endif

    MADI_DPUTS2("context resumed");
}

static size_t select_victim_randomly(uth_comm& c)
{
    madi::pid_t me = c.get_pid();
    size_t n_procs = c.get_n_procs();

    madi::pid_t pid;
    do {
        pid = (madi::pid_t)random_int((int)n_procs);
        MADI_CHECK((size_t)pid != n_procs);
    } while (pid == me);

    return pid;
}

static size_t select_victim(uth_comm& c)
{
    return select_victim_randomly(c);
}

bool worker::steal_with_lock(taskq_entry *entry,
                             madi::pid_t *victim,
                             taskque **taskq_ptr)
{
    uth_comm& c = madi::proc().com();

    size_t target = select_victim(c);
    taskq_entry *entries = taskq_entries_array_[target];
    taskque *taskq = taskq_array_[target];
    
#define MADI_ABORTING_STEAL 1
#if MADI_ABORTING_STEAL

    long t0 = rdtsc();

    bool do_abort = taskq->empty(c, target, taskq_buf_);

    long t1 = rdtsc();
    g_prof->current_steal().empty_check = t1 - t0;

    if (do_abort) {
        g_prof->n_aborted_steals += 1;
        return false;
    }
#endif

    long t2 = rdtsc();

    bool success;
    success = taskq->steal_trylock(c, target);

    long t3 = rdtsc();
    g_prof->current_steal().lock = t3 - t2;

    if (!success) {
        g_prof->n_failed_steals_lock += 1;
        return false;
    }

    success = taskq->steal(c, target, entries, entry, taskq_buf_);

    long t4 = rdtsc();
    g_prof->current_steal().steal = t4 - t3;

    if (!success) {
        taskq->steal_unlock(c, target);

        long t5 = rdtsc();
        g_prof->current_steal().unlock = t5 - t4;
        g_prof->n_failed_steals_empty += 1;

        return false;
    }

    g_prof->n_success_steals += 1;

    *victim = target;
    *taskq_ptr = taskq;  // for unlock when task stack is transfered
    return true;
}

void resume_context(saved_context *sctx, context *ctx)
{
    madi::current_worker().waitq().push_back(sctx);

    MADI_RESUME_CONTEXT(ctx);
}

void resume_saved_context(saved_context *sctx, saved_context *next_sctx)
{
    worker& w = madi::current_worker();

    if (sctx != NULL)
        w.waitq().push_back(sctx);

    w.is_main_task_ = next_sctx->is_main_task;

    uint8_t *next_stack_top = (uint8_t *)next_sctx->sp - 128;

    MADI_EXECUTE_ON_STACK(madi_worker_do_resume_saved_context,
                          next_sctx, sctx, NULL, NULL,
                          next_stack_top);
}

void resume_remote_context(saved_context *sctx, 
                           std::tuple<taskq_entry *, madi::pid_t,
                                      taskque *, tsc_t> *arg)
/*
                           taskq_entry* entry,
                           madi::pid_t victim, 
                           taskque *taskq)
*/
{
    long t0 = std::get<3>(*arg);
    long t1 = rdtsc();
    g_prof->current_steal().suspend = t1 - t0;

    std::get<3>(*arg) = t1;

    taskq_entry *entry = std::get<0>(*arg);

    worker& w = madi::current_worker();

    MADI_ASSERT(sctx != NULL);

    w.waitq().push_back(sctx);

    w.is_main_task_ = false;

    uint8_t *next_stack_top = (uint8_t *)entry->frame_base;
    MADI_EXECUTE_ON_STACK(madi_worker_do_resume_remote_context,
                          sctx, arg, NULL, NULL,
                          next_stack_top);
}

void worker::do_scheduler_work()
{
    taskq_entry *entry = taskq_->pop();

    MADI_UTH_COMM_POLL();

    if (entry != NULL) {
        // switch to the parent task
        MADI_ASSERT(!is_main_task_);
        MADI_DPUTSB2("resuming the parent task");
        suspend(resume_context, entry->ctx);
    } else if (!is_main_task_ && main_ctx_ != NULL) {
        // if this task is not the main task,
        // and the main task is not suspended (the frames are on the stack),
        // switch to the main task
        is_main_task_ = true;
        MADI_DPUTSB2("resuming the main task");
        suspend(resume_context, main_ctx_);
    } else {
//        MADI_ASSERT(is_main_task_);

        // start work stealing 

        madi::pid_t victim;

        taskq_entry& stolen_entry = *taskq_entry_buf_;
        taskque *taskq;
        bool success = steal_with_lock(&stolen_entry, &victim, &taskq);

        if (success) {
            // next_steal() is called when stolen thread resumed.
        } else {
            // do not record when a steal fails.
            //g_prof->next_steal();
        }

        if (success) {

            main_ctx_ = NULL;

            // switch to the stolen task
            MADI_DPUTSB2("resuming a stolen task");

            tsc_t t = rdtsc();

            std::tuple<taskq_entry *, madi::pid_t, taskque *, tsc_t> 
                arg(&stolen_entry, victim, taskq, t);

            suspend(resume_remote_context, &arg);
        } else if (!waitq_.empty()) {
            main_ctx_ = NULL;

            // switch to a waiting task
            MADI_DPUTSB2("resuming a waiting task");
            saved_context *sctx = waitq_.front();
            waitq_.pop_front();
            suspend(resume_saved_context, sctx);
        } else {
            // do nothing
        }
    }
}

void worker::notify_done()
{
    done_ = true;
}

}

extern "C" {

void madi_resume_context(context *ctx)
{
    //MADI_CONTEXT_ASSERT(ctx);

    MADI_RESUME_CONTEXT(ctx);
}

__attribute__((noinline))
void madi_worker_do_resume_saved_context(void *p0, void *p1, void *p2, void *p3)
{
    saved_context *sctx = (saved_context *)p0;

    MADI_DEBUG3({
        saved_context *prev_sctx = (saved_context *)p1;
        if (prev_sctx != NULL)
            memset(prev_sctx->stack_top, 0xFF, prev_sctx->stack_size);
    });

    context *ctx = sctx->ctx;

    void *rsp;
    MADI_GET_CURRENT_SP(&rsp);
    MADI_DPUTS2("current rsp      = %p", rsp);

    MADI_SCONTEXT_PRINT(2, sctx);
    MADI_SCONTEXT_ASSERT(sctx);

    uint8_t *frame_base = sctx->stack_top;
    size_t frame_size = sctx->stack_size;

    memcpy(frame_base, sctx->partial_stack, frame_size);
   
    MADI_CONTEXT_PRINT(2, ctx);
    if (!sctx->is_main_task)
        MADI_CONTEXT_ASSERT(ctx);

    MADI_ASSERT(sctx->ip == ctx->instr_ptr());
    MADI_ASSERT(sctx->sp == ctx->stack_ptr());

    free((void *)sctx);

    MADI_DPUTSR2("resuming  [%p, %p) (size = %zu) (waiting)",
                 frame_base, frame_base + frame_size, frame_size);

    madi_resume_context(ctx);
}

static void print(uint8_t *p)
{
    MADI_DPUTS("p = %p", p);
}

__attribute__((noinline))
void madi_worker_do_resume_remote_context_1(uth_comm& c,
                                            madi::pid_t victim,
                                            taskque *taskq,
                                            taskq_entry *entry,
                                            tsc_t t0)
{
    uint8_t *frame_base = (uint8_t *)entry->frame_base;
    size_t frame_size = entry->frame_size;

    context *ctx = entry->ctx;

    MADI_TENTRY_PRINT(2, entry);
    MADI_CONTEXT_PRINT(2, ctx);
    MADI_CONTEXT_PRINT(2, entry->ctx->parent);

//    MADI_TENTRY_ASSERT(entry);
//    MADI_CONTEXT_ASSERT(ctx);

    long t1 = rdtsc();
    prof_steal_entry& e = g_prof->current_steal();
    e.stack_transfer = t1 - t0;
    e.ctx = (void *)ctx;
    e.frame_size = frame_size;
    e.me = c.get_pid();
    e.victim = victim;

    taskq->steal_unlock(c, victim);

    long t2 = rdtsc();
    e.unlock = t2 - t1;
    e.tmp = t2;

    MADI_DPUTSR1("resuming  [%p, %p) (size = %zu) (stolen)",
                 frame_base, frame_base + frame_size, frame_size);

    madi_resume_context(ctx);
}


__attribute__((noinline))
void madi_worker_do_resume_remote_context(void *p0, void *p1, void *p2,
                                          void *p3)
{
    // data pointed from the parameter pointers may be corrupted
    // by stack copy, so we have to copy it to the current stack frame.

    MADI_DEBUG3( saved_context *prev_sctx = (saved_context *)p0 );

    std::tuple<taskq_entry *, madi::pid_t, taskque *, tsc_t>& arg =
       *(std::tuple<taskq_entry *, madi::pid_t, taskque *, tsc_t> *)p1;
   
    taskq_entry entry = *std::get<0>(arg);
    madi::pid_t victim = std::get<1>(arg);
    taskque *taskq = std::get<2>(arg);
    tsc_t t0 = std::get<3>(arg);
   
    iso_space& ispace = madi::proc().ispace();
    uth_comm& c = madi::proc().com();
    madi::pid_t me = c.get_pid();

    uint8_t *frame_base = (uint8_t *)entry.frame_base;
    size_t frame_size = entry.frame_size;

    MADI_DPUTSR1("RDMA region [%p, %p) (size = %zu)",
                frame_base, (uint8_t *)frame_base + frame_size,
                frame_size);
    MADI_TENTRY_PRINT(2, &entry);

    MADI_DEBUG3({
        memset(prev_sctx->stack_top, 1, prev_sctx->stack_size);
    });

    uint8_t *remote_base = (uint8_t *)ispace.remote_ptr(frame_base, victim);
    uint8_t *local_base = (uint8_t *)ispace.remote_ptr(frame_base, me);

    // alignment for RDMA operations
    local_base  = (uint8_t *)((uintptr_t)local_base  & ~0x3);
    remote_base = (uint8_t *)((uintptr_t)remote_base & ~0x3);
    frame_size = (frame_size + 4) / 4 * 4;

    MADI_DEBUG2({
        MADI_DPUTS2("RDMA_GET(%p(%p), %p, %zu)",
                    local_base, frame_base, remote_base, frame_size);

        uint8_t *sp;
        MADI_GET_CURRENT_SP(&sp);

        MADI_DPUTS("sp = %p", sp);

        MADI_ASSERT(sp <= frame_base);
    });

#define MADI_SHMEM 0
#if MADI_SHMEM
    memcpy(frame_base, remote_base, frame_size);
#else
    c.reg_get(local_base, remote_base, frame_size, victim);
#endif

    madi_worker_do_resume_remote_context_1(c, victim, taskq, &entry,
                                           t0);
}

}

