// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_string.h>

namespace nx::sdk {

/** Error codes used by Plugin methods. */
enum class ErrorCode: int
{
    noError = 0,
    networkError = -22,
    unauthorized = -1,
    internalError = -1000, //< Assertion-failure-like error.
    invalidParams = -1001, //< Method arguments are invalid.
    notImplemented = -21,
    otherError = -100,
    ioError = -31,
    noData = -101, //< Call succeeded, but no valid data can be returned (EoF for example)
};

class Error
{
public:
    Error(ErrorCode errorCode, const IString* errorMessage):
        m_errorCode(errorCode), m_errorMessage(errorMessage)
    {
    }

    bool isOk() const
    {
        return m_errorCode == ErrorCode::noError && m_errorMessage == nullptr;
    }

    ErrorCode errorCode() const { return m_errorCode; }
    const IString* errorMessage() const { return m_errorMessage; }

    Error(Error&&) = default;
    Error& operator=(const Error&) = default;

private:
    ErrorCode m_errorCode;
    const IString* m_errorMessage;
};

template<typename Value>
class Result
{
public:
    Result(): m_error(ErrorCode::noError, nullptr) {}

    Result(Value value): m_error(ErrorCode::noError, nullptr), m_value(std::move(value)) {}

    Result(Error&& error): m_error(std::move(error)) {}

    Result& operator=(Error&& error)
    {
        m_error = std::move(error);
        m_value = Value{};
        return *this;
    }

    Result& operator=(Value value)
    {
        m_error = Error{ErrorCode::noError, nullptr};
        m_value = value;
        return *this;
    }

    bool isOk() const { return m_error.isOk(); }

    const Error& error() const { return m_error; }
    Value value() const { return m_value; }

private:
    Error m_error;
    Value m_value{};
};

template<>
class Result<void>
{
public:
    Result(): m_error(ErrorCode::noError, nullptr) {}

    Result(Error&& error): m_error(std::move(error)) {}

    Result& operator=(Error&& error)
    {
        m_error = std::move(error);
        return *this;
    }

    bool isOk() const { return m_error.isOk(); }

    const Error& error() const { return m_error; }

private:
    Error m_error;
};

} // namespace nx::sdk
