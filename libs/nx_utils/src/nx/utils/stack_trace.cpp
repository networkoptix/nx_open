// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stack_trace.h"

#include <cpptrace/cpptrace.hpp>

#include <nx/build_info.h>
#include <nx/utils/log/format.h>

namespace nx {

std::string stackTrace()
{
    return cpptrace::generate_trace().to_string();
}

} // namespace nx
