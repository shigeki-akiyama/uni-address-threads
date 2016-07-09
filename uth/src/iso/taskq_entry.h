#ifndef MADI_ISO_STACK_TASKQ_ENTRY_H
#define MADI_ISO_STACK_TASKQ_ENTRY_H

#include "context.h"

namespace madi {

    class taskq_entry {
    public:
        context ctx_;
    public:
        taskq_entry() : ctx_() {}
        taskq_entry(context ctx) : ctx_(ctx) {}
        ~taskq_entry() {}

        taskq_entry(const taskq_entry& th) : ctx_(th.ctx_) {}
        const taskq_entry& operator=(const taskq_entry& th) {
            ctx_ = th.ctx_;
            return *this;
        }
   };
    
}

#endif
