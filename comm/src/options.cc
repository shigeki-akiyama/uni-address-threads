#include "options.h"
#include "madm_comm_config.h"
#include "madm_debug.h"
#include <cstdlib>
#include <unistd.h>

namespace madi {
namespace comm { 

    // default values of configuration variables
    struct options options = {
        8192,                           // page_size
        MADI_DEFAULT_N_CORES,           // n_procs_per_node
        MADI_DEFAULT_SERVER_MOD,        // server_mod
        10,            // n_max_sends (heuristics: ~ # of cores within a node)
        0,                              // gasnet_poll_thread
        5,             // debug level (only if configured with debug option)
    };

    template <class T>
    void set_option(const char *name, T *value);

    template <>
    void set_option<int>(const char *name, int *value)
    {
        char *s = getenv(name);
        if (s != NULL)
            *value = atoi(s);
    }

    template <>
    void set_option<size_t>(const char *name, size_t *value)
    {
        char *s = getenv(name);

        if (s != NULL)
            *value = static_cast<size_t>(atol(s));
    }

    void options_initialize()
    {
        long page_size = sysconf(_SC_PAGE_SIZE);
        options.page_size = static_cast<size_t>(page_size);

        // FIXME: auto detect
        set_option("MADM_CORES", &options.n_procs_per_node);

        // default value of SERVER_MOD is MADM_CORES in FX10
        if (options.server_mod > 0)
            options.server_mod = options.n_procs_per_node;

        set_option("MADM_SERVER_MOD", &options.server_mod);
        set_option("MADM_GASNET_POLL_THREAD", &options.gasnet_poll_thread);
        set_option("MADM_DEBUG_LEVEL", &options.debug_level);

        // validate server_mod
        MADI_CHECK(options.server_mod <= options.n_procs_per_node);
    }

    void options_finalize()
    {
    }

}
}
