#pragma once

#include <optional>
#include <type_traits>

#include "udt.h"

/**
 * Is used to report any error within library except the public API
 * which follows C API style by returning int (0 - success, -1 - error).
 * Intended to protect against mixing int and bool return codes and misused if (result) {...}.
 * @param Payload Result value.
 */
template<typename Payload = void>
class Result:
    public Result<void>
{
    using base_type = Result<void>;

public:
    /**
     * Constructs success result.
     */
    explicit Result(Payload payload):
        m_payload(std::move(payload))
    {
    }

    /**
     * Constructs error result.
     */
    Result(ErrorInfo errorInfo):
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
class Result<void>
{
public:
    /**
     * Constructs success result.
     */
    Result() = default;

    /**
     * Constructs error result.
     */
    Result(ErrorInfo errorInfo):
        m_ok(false),
        m_errorInfo(std::move(errorInfo))
    {
    }

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    Result(Result&&) = default;
    Result& operator=(Result&&) = default;

    bool ok() const { return m_ok; }

    /**
     * Behavior is undefined in case of success result.
     */
    const ErrorInfo& errorInfo() const { return *m_errorInfo; }

private:
    bool m_ok = true;
    std::optional<ErrorInfo> m_errorInfo;
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
