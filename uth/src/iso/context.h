#ifndef MADI_ISO_STACK_CONTEXT_H
#define MADI_ISO_STACK_CONTEXT_H

#include "stack_pool.h"
#include "myth_context.h"
#include "misc.h"
#include "debug.h"
#include <stdint.h>
#include <cstdio>

namespace madi {

    class context {

        struct context_header {
            myth_context saved_mctx;
            size_t stack_size;
        };

        void *stack_base_;

        context_header *header()
          { return reinterpret_cast<context_header *>(stack_base_); }

        bool valid() { return stack_base_ != NULL; }

    public:
        context() : stack_base_(NULL) {}

        context(const context& ctx) : stack_base_(ctx.stack_base_) {}
        context& operator=(const context& ctx)
          { stack_base_ = ctx.stack_base_; return *this;  }

        context(size_t stack_size, stack_allocator& salloc) : 
            stack_base_(NULL)
        {
            // stack_size must be sizeof(void *)-aligned
            MADI_ASSERT((stack_size & 0xF) == 0);

            // FIXME
            size_t actual_stack_size = (stack_size == 0) ? 4096 : stack_size;

            void *stack = salloc.allocate(actual_stack_size);

            uint8_t *p = reinterpret_cast<uint8_t *>(stack)
                         + actual_stack_size - sizeof(context_header);

            void *stack_base = reinterpret_cast<void *>(p);
            context_header *h = reinterpret_cast<context_header *>(p);

            if (stack_size > 0) {
                // initialization for a user-level thread
                myth_make_context_empty(&h->saved_mctx, stack_base, stack_size);
                h->stack_size = actual_stack_size;
            } else {
                // initialization for a OS thread
                h->saved_mctx.rsp = 0;
                h->stack_size = 0;
            }

            stack_base_ = stack_base;
        }

        void destroy(stack_allocator& salloc)
        {
            context_header *h = header();

            void *stack = reinterpret_cast<uint8_t *>(h) 
                          + sizeof(context_header) - h->stack_size;

            salloc.deallocate(stack, h->stack_size);
        }

        template <class F, class... Args>
        static void switch_to_callback(void *arg0, void *arg1, void *arg2)
        {
            F callback = *reinterpret_cast<F *>(arg0);
            std::tuple<context&, Args...> targs = 
                *reinterpret_cast<std::tuple<context&, Args...> *>(arg1);

            tuple_apply<void>::f(callback, targs);
        }

        template <class F, class... Args>
        void switch_to(context new_ctx, F callback, Args... args)
        {
            std::tuple<context&, Args...> targs(*this, args...);

            myth_context *from_mctx = &this->header()->saved_mctx;
            myth_context *to_mctx = &new_ctx.header()->saved_mctx;

            MADI_ASSERT(from_mctx != NULL);
            MADI_ASSERT(to_mctx != NULL);

            myth_swap_context_withcall_s(
                from_mctx, to_mctx,
                switch_to_callback<F, Args...>,
                reinterpret_cast<void *>(&callback),
                reinterpret_cast<void *>(&targs),
                NULL);
        }
    };

}

#endif
