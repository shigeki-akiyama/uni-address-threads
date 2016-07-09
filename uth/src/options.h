#ifndef MADI_OPTIONS_H
#define MADI_OPTIONS_H

#include <cstddef>

namespace madi {

    struct options {
        size_t root_stack_size;
        size_t nonroot_stack_size;
        size_t stack_overflow_detection;
        size_t taskq_capacity;
        size_t page_size;
    };

    extern options options;

    void options_initialize();
    void options_finalize();
}

#endif
