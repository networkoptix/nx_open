// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>

#if defined(__arm__) //< Some 32-bit ARM devices lack the kernel support for the atomic int64.
    typedef uint32_t SocketSequence;
#else
    typedef uint64_t SocketSequence;
#endif
