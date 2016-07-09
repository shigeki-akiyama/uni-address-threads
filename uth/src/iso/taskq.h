#ifndef MADI_ISO_STACK_TASKQUE_H
#define MADI_ISO_STACK_TASKQUE_H

#include <deque>
#include "taskq_entry.h"

namespace madi {

    class taskque {
        std::deque<taskq_entry> deque_;
    public:
        taskque() : deque_() {}
        ~taskque() {}

        void initialize(size_t capacity);
        void finalize();

        void push(const taskq_entry& th);
        bool pop(taskq_entry *th);
        bool steal(taskq_entry *th);
    };
}


#endif
