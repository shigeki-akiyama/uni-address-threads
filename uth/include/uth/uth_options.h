#ifndef MADI_UTH_OPTIONS_H
#define MADI_UTH_OPTIONS_H

#include <cstddef>

namespace madi {

    struct uth_options {
        size_t stack_size;
        size_t stack_overflow_detection;
        size_t taskq_capacity;
        size_t page_size;
        int    profile_enabled;
    };

    extern uth_options uth_options;

    void uth_options_initialize();
    void uth_options_finalize();
}

#endif
