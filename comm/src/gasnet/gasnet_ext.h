#ifndef MADI_COMM_GASNET_EXT_H
#define MADI_COMM_GASNET_EXT_H

// for suppressing warnings
#define GASNETT_USE_GCC_ATTRIBUTE_MAYALIAS 1
#undef PLATFORM_COMPILER_FAMILYNAME
#undef PLATFORM_COMPILER_FAMILYID
#undef PLATFORM_COMPILER_VERSION
#undef PLATFORM_COMPILER_VERSION_STR
#undef __PLATFORM_COMPILER_GNU_VERSION_STR

#include <gasnet.h>

#undef PLATFORM_COMPILER_FAMILYNAME
#undef PLATFORM_COMPILER_FAMILYID
#undef PLATFORM_COMPILER_VERSION
#undef PLATFORM_COMPILER_VERSION_STR
#undef __PLATFORM_COMPILER_GNU_VERSION_STR

#endif
