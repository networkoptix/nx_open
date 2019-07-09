// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx {
namespace sdk {

/** Error codes used both by Plugin methods and Server callbacks. */
enum class ErrorCode: int
{
    noError = 0,
    networkError = -22,
    unauthorized = -1,
    internalError = -1000,
    invalidParams = -1001,
    notImplemented = -21,
    otherError = -100,
};

} // namespace sdk
} // namespace nx
