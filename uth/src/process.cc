#include "process.h"

#include <cstdio>
#include "uth_options.h"
#include "iso_space-inl.h"
#include "madi-inl.h"
#include "debug.h"
#include "uth.h"

namespace madi {

    process::process() :
        initialized_(false),
        comm_(), ispace_(), debug_out_(stderr),
        workers_()
    {
    }

    process::~process()
    {
    }

    bool process_do_initialize(process& self, int& argc, char **& argv)
    {
        self.ispace_.initialize(self.comm_);

        MADI_DPUTS2("iso-address space initialized");

        self.workers_[0].initialize(self.comm_);

        MADI_DPUTS2("worker initialized");

        self.initialized_ = true;

        MADI_DPUTS2("MassiveThreads/DM system initialized");

        native_barrier();

        return self.initialized_;
    }

    void process_do_finalize(process& self)
    {
        native_barrier();

        MADI_DPUTS2("start finalization");

        self.initialized_ = false;

        self.workers_[0].finalize(self.comm_);

        MADI_DPUTS2("worker finalized");

        self.ispace_.finalize(self.comm_);

        MADI_DPUTS2("iso-address space finalized");
    }

    bool process::initialize(int& argc, char **& argv)
    {
        debug_out_ = stderr;

        uth_options_initialize();

        g_prof = new prof();

        bool result = comm_.initialize(argc, argv);

        if (!result)
            return false;

        MADI_DPUTS2("communication layer initialized");

        // FIXME: return do_initialize(*this, argc, argv);

        return true;
    }

    void process::finalize()
    {
        g_prof->output();
        delete g_prof;

        // FIXME: do_finalize(*this);

        comm_.finalize();

        MADI_DPUTS2("communication layer finalized");

        uth_options_finalize();

        MADI_DPUTS2("MassiveThreads/DM system finalized");
    }

}
