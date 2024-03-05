// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stack_trace.h"

#include <sstream>

#if defined(__APPLE__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <boost/stacktrace.hpp>

namespace nx {

std::string stackTrace()
{
    std::stringstream ss;
    ss << boost::stacktrace::stacktrace();
    return ss.str();
}

} // namespace nx
