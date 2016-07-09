#ifndef MADI_COMM_CONFIG_H
#define MADI_COMM_CONFIG_H

#include <climits>

#define MADI_COMM_LAYER_SEQ    0
#define MADI_COMM_LAYER_SHMEM  1
#define MADI_COMM_LAYER_MPI3   2
#define MADI_COMM_LAYER_GASNET 3
#define MADI_COMM_LAYER_FX10   4


#undef MADI_DEBUG_LEVEL
#undef MADI_COMM_LAYER
#undef PACKAGE
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME

#include "madm_comm_acconfig.h"

#undef PACKAGE
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME


#if MADI_COMM_LAYER == MADI_COMM_LAYER_SHMEM

#define MADI_DEFAULT_N_CORES            (INT_MAX)
#define MADI_DEFAULT_SERVER_MOD         (0)
#define MADI_NEED_POLL                  (0)

#elif MADI_COMM_LAYER == MADI_COMM_LAYER_MPI3

#define MADI_DEFAULT_N_CORES            (16)
#define MADI_DEFAULT_SERVER_MOD         (0)
#define MADI_NEED_POLL                  (1)

#elif MADI_COMM_LAYER == MADI_COMM_LAYER_GASNET

#define MADI_DEFAULT_N_CORES            (16)
#define MADI_DEFAULT_SERVER_MOD         (0)
#define MADI_NEED_POLL                  (1)

#elif MADI_COMM_LAYER == MADI_COMM_LAYER_FX10

#define MADI_DEFAULT_N_CORES            (16)
#define MADI_DEFAULT_SERVER_MOD         (16)
#define MADI_NEED_POLL                  (0)

#else

#error "unknown communication layer"

#endif

#endif
