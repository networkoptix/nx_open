#pragma once

#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace nx::cloud::aws {

enum class ResultCode
{
    ok = 0,
    unauthorized,
    networkError,
    error,
    notImplemented,
};

constexpr std::string_view toString(ResultCode code)
{
    switch (code)
    {
        case ResultCode::ok: return "ok";
        case ResultCode::unauthorized: return "unauthorized";
        case ResultCode::networkError: return "networkError";
        case ResultCode::error: return "error";
        case ResultCode::notImplemented: return "notImplemented";
    }

    return "unknown";
}

inline std::ostream& operator<<(std::ostream& os, ResultCode code)
{
    return os << toString(code);
}

struct Result
{
    constexpr Result(ResultCode code = ResultCode::ok): m_code(code) {}

    Result(ResultCode code, std::string text):
        m_code(code),
        m_text(std::move(text))
    {
    }

    constexpr ResultCode code() const { return m_code; }
    constexpr bool ok() const { return m_code == ResultCode::ok; }

    std::string text() const
    {
        return m_text ? *m_text : std::string(toString(m_code));
    }

private:
    ResultCode m_code = ResultCode::ok;
    std::optional<std::string> m_text;
};

} // namespace nx::cloud::aws
