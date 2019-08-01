// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx {
namespace sdk {

/**
 * Error codes used both by Plugin methods and Server callbacks.
 *
 * ATTENTION: The values match error constants in <camera/camera_plugin.h>.
 */
enum class Error: int
{
    noError = 0,
    unknownError = -100,
    networkError = -22,
};

} // namespace sdk
} // namespace nx
