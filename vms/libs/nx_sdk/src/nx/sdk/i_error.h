// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/i_string_map.h>

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

/**
 * Class for error reporting. Usually passed to some methods and expected to be properly filled in a
 * case of an error.
 */
class IError: public Interface<IError>
{
public:
    /**
     * Sets general information about an error.
     *
     * @param errorCode An error code most suitable for an error that occurred.
     * @param message Human-readable error description.
     */
    virtual void setError(ErrorCode errorCode, const char* message) = 0;

    /**
     * Sets a detail about an error. Some methods can have specific requirements about what
     * should be placed in the details map. Such requirements can be found in a documentation for
     * the method.
     */
    virtual void setDetail(const char* key, const char* message) = 0;

    virtual ErrorCode errorCode() const = 0;
    virtual const IString* message() const = 0;
    virtual const IStringMap* details() const = 0;
};

} // namespace sdk
} // namespace nx
