// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/error_code.h>
#include <nx/sdk/helpers/string.h>

namespace nx {
namespace sdk {

struct Error
{
    Error(ErrorCode errorCode, const IString* errorMessage):
        errorCode(errorCode), errorMessage(errorMessage)
    {
    }

    const ErrorCode errorCode;
    const IString* const errorMessage;
};

template<typename Value>
struct Result
{
    Result(Value value): error(ErrorCode::noError, nullptr), value(std::move(value)) {}

    Result(Error&& error): error(std::move(error)) {}

    bool isOk() const
    {
        return error.errorCode == ErrorCode::noError && error.errorMessage == nullptr;
    }

    const Error error;
    const Value value{};
};

template<>
struct Result<void>
{
    Result(): error(ErrorCode::noError, nullptr) {}
    Result(Error&& error): error(std::move(error)) {}

    bool isOk() const
    {
        return error.errorCode == ErrorCode::noError && error.errorMessage == nullptr;
    }

    const Error error;
};

} // namespace sdk
} // namespace nx
