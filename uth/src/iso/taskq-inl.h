#ifndef MADI_ISO_STACK_TASKQUE_INL_H
#define MADI_ISO_STACK_TASKQUE_INL_H

#include "taskq.h"

namespace madi {

    inline void taskque::initialize(size_t capacity) {
        deque_.resize(capacity);
    }

    inline void taskque::finalize() {
        deque_.resize(0);
    }

    inline void taskque::push(const taskq_entry& th) {
        deque_.push_back(th);
    }

    inline bool taskque::pop(taskq_entry *th) {
        if (deque_.empty()) {
            return false;
        } else {
            *th = deque_.back();
            deque_.pop_back();
            return true;
        }
    }

    inline bool taskque::steal(taskq_entry *th) {
        if (deque_.empty()) {
            return false;
        } else {
            *th = deque_.front();
            deque_.pop_front();
            return true;
        }
    }
    
}

#endif
