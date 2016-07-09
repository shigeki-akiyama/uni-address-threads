#include "options.h"
#include <cstdlib>
#include <unistd.h>

namespace madi {

    // default values of configuration variables
    struct options options = {
        1024 * 1024,        // root_stack_size
        0,                  // nonroot_stack_size
        1,                  // stack_overflow_detection
        1024,               // taskq_capacity
        8192,               // page_size
    };

    template <class T>
    void set_option(const char *name, T *value);

    template <>
    void set_option<int>(const char *name, int *value) {
        char *s = getenv(name);
        if (s != NULL)
            *value = atoi(s);
    }

    template <>
    void set_option<size_t>(const char *name, size_t *value) {
        char *s = getenv(name);

        if (s != NULL) {
            *value = static_cast<size_t>(atol(s));
        }
    }

    void options_initialize()
    {
        set_option("MADM_ROOT_STACK_SIZE", &options.root_stack_size);
        set_option("MADM_NONROOT_STACK_SIZE", &options.nonroot_stack_size);
        set_option("MADM_STACK_DETECT", &options.stack_overflow_detection);
        set_option("MADM_TASKQ_CAPACITY", &options.taskq_capacity);

        long page_size = sysconf(_SC_PAGE_SIZE);
        options.page_size = static_cast<size_t>(page_size);
    }

    void options_finalize()
    {
    }

}
