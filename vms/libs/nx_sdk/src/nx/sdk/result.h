// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_string.h>

namespace nx {
namespace sdk {

/** Error codes used both by Plugin methods and Server callbacks. */
enum class ErrorCode: int
{
    noError = 0,
    networkError = -22,
    unauthorized = -1,
    internalError = -1000, //< Assertion-failure-like error.
    invalidParams = -1001, //< Method arguments are invalid.
    notImplemented = -21,
    otherError = -100,
};

struct Error
{
    Error(ErrorCode errorCode, const IString* errorMessage):
        errorCode(errorCode), errorMessage(errorMessage)
    {
    }

    bool isOk() const
    {
        return errorCode == ErrorCode::noError && errorMessage == nullptr;
    }

    const ErrorCode errorCode;
    const IString* const errorMessage;
};

template<typename Value>
struct Result
{
    Result(Value value): error(ErrorCode::noError, nullptr), value(std::move(value)) {}
    Result(Error&& error): error(std::move(error)) {}
    bool isOk() const { return error.isOk(); }

    const Error error;
    const Value value{};
};

template<>
struct Result<void>
{
    Result(): error(ErrorCode::noError, nullptr) {}
    Result(Error&& error): error(std::move(error)) {}
    bool isOk() const { return error.isOk(); }

    const Error error;
};

} // namespace sdk
} // namespace nx
