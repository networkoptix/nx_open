// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/i_string_map.h>

namespace nx {
namespace sdk {

/**
 * Error codes used both by Plugin methods and Server callbacks.
 *
 * ATTENTION: The values match error constants in <camera/camera_plugin.h>.
 */
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

class IError: public Interface<IError>
{
public:
    virtual void setError(ErrorCode errorCode, const char* message) = 0;
    virtual void setDetail(const char* key, const char* message) = 0;

    virtual ErrorCode errorCode() const = 0;
    virtual const IString* message() const = 0;
    virtual const IStringMap* details() const = 0;
};

} // namespace sdk
} // namespace nx
