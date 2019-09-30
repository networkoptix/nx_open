#pragma once

#include <optional>
#include <type_traits>

#include "udt.h"

// TODO: #ak The following using should be removed.
using UDT::Error;

class BasicResult
{
public:
    /**
     * Constructs success result.
     */
    BasicResult() = default;

    /**
     * Constructs error result.
     */
    BasicResult(Error error):
        m_ok(false),
        m_error(std::move(error))
    {
    }

    BasicResult(const BasicResult&) = delete;
    BasicResult& operator=(const BasicResult&) = delete;
    BasicResult(BasicResult&&) = default;
    BasicResult& operator=(BasicResult&&) = default;

    bool ok() const { return m_ok; }

    /**
     * Behavior is undefined in case of success result.
     */
    const Error& error() const { return *m_error; }

private:
    bool m_ok = true;
    std::optional<Error> m_error;
};

//-------------------------------------------------------------------------------------------------

/**
 * Intended to report any error within library except the public API
 * which follows C API style by returning int (0 - success, -1 - error).
 * Intended to protect against mixing int and bool return codes and misused if (result) {...}.
 * @param Payload Result value.
 */
template<typename Payload = void>
class Result:
    public BasicResult
{
    using base_type = BasicResult;

public:
    /**
     * Constructs success result.
     */
    Result(Payload payload):
        m_payload(std::move(payload))
    {
    }

    /**
     * Constructs error result.
     */
    Result(Error errorInfo):
        base_type(std::move(errorInfo))
    {
    }

    /**
     * Behavior is undefined if the result holds an error.
     */
    Payload take()
    {
        auto payload = std::exchange(m_payload, std::nullopt);
        return std::move(*payload);
    }

    /**
     * Behavior is undefined if the result holds an error.
     */
    const Payload& get() const
    {
        return *m_payload;
    }

private:
    std::optional<Payload> m_payload;
};

//-------------------------------------------------------------------------------------------------

template<>
class Result<void>:
    public BasicResult
{
    using base_type = BasicResult;

public:
    using base_type::base_type;
};

//-------------------------------------------------------------------------------------------------

inline Result<> success()
{
    return Result<>();
}

template<typename Payload>
Result<std::decay_t<Payload>> success(Payload&& payload)
{
    return Result<std::decay_t<Payload>>(std::forward<Payload>(payload));
}
