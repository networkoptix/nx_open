// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "json.h"

/**@file
 * Wrapper for the original Json11 .cpp. Puts class Json into namespace nx::kit.
 */

// Includes from json11.hpp are duplicated here to put them in the global namespace.
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <limits>

namespace nx {
namespace kit {

namespace detail
{
    #include "../../json11/json11.cpp" //< Original Json11 .cpp.
}

} // namespace kit
} // namespace nx
