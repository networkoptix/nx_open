// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rlimit.h"

namespace nx {
namespace utils {
namespace rlimit {

unsigned long getMaxFileDescriptors()
{
    return 0;
}

unsigned long setMaxFileDescriptors(unsigned long value)
{
    return value;
}

} // namespace rlimit
} // namespace utils
} // namespace nx
