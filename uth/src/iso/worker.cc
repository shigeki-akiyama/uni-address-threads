#include "worker.h"
#include "madi.h"
#include "debug.h"

#include "worker-inl.h"
#include "taskq-inl.h"
#include "madi-inl.h"

using namespace madi;

worker::worker() :
    taskq_(), current_ctx_(), main_ctx_(), 
    done_(false), main_waiting_(false),
    salloc_()
{
}

worker::~worker()
{
}

worker::worker(const worker& w) :
    taskq_(w.taskq_), current_ctx_(w.current_ctx_), main_ctx_(w.main_ctx_),
    done_(w.done_), main_waiting_(w.main_waiting_),
    salloc_(w.salloc_)
{
//    MADI_NOT_REACHED;
}

void worker::initialize()
{
    taskq_.initialize(madi::config.taskq_capacity);

    context ctx(0, salloc_);
 
    current_ctx_ = ctx;
    main_ctx_ = ctx;
}

void worker::finalize()
{
}

void worker::switch_to_main()
{
    switch_to(main_ctx_, exit_callback, this);
}

void worker::start_callback(context old_ctx, context new_ctx,
                               worker *self,
                               void (*f)(int, char **),
                               int argc, char **argv)
{
    self->main_waiting_ = true;

    f(argc, argv);

    self->notify_done();

    self->switch_to_main();
}

void worker::start(void (*f)(int, char **), int argc, char **argv)
{
    size_t stack_size = 8192; // FIXME
    context new_ctx(stack_size, salloc_);

    switch_to(new_ctx, start_callback, new_ctx, this, f, argc, argv);

    // scheduler loop until notify_done() is called
    while (!done_)
        do_scheduler_work();
}

static size_t select_victim()
{
    MADI_UNDEFINED;
}

context worker::steal()
{
    size_t victim = select_victim();
    victim++;
//    return comm.steal(victim);
    MADI_UNDEFINED;
}

void worker::notify_done()
{
    done_ = true;
}

