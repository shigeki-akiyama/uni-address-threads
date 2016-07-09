#include "uth_options.h"
#include <cstdlib>
#include <unistd.h>

namespace madi {

    // default values of configuration variables
    struct uth_options uth_options = {
        256 * 1024,         // stack_size
        1,                  // stack_overflow_detection
        1024,               // taskq_capacity
        8192,               // page_size
        0,                  // profile_enabled
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

    void uth_options_initialize()
    {
        set_option("MADM_STACK_SIZE", &uth_options.stack_size);
        set_option("MADM_STACK_DETECT", &uth_options.stack_overflow_detection);
        set_option("MADM_TASKQ_CAPACITY", &uth_options.taskq_capacity);
        set_option("MADM_PROFILE", &uth_options.profile_enabled);

        long page_size = sysconf(_SC_PAGE_SIZE);
        uth_options.page_size = static_cast<size_t>(page_size);
    }

    void uth_options_finalize()
    {
    }

}
