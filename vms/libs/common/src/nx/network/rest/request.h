#pragma once

#include "params.h"

// TODO: Should be moved into nx::network::rest.
class QnRestConnectionProcessor;

namespace nx::network::rest {

struct Request
{
    // TODO: These should be private, all data should be avalible by getters.
    const http::Request* httpRequest = nullptr;
    const QnRestConnectionProcessor* owner = nullptr;

    std::optional<Content> content;

    Request(const http::Request* httpRequest, const QnRestConnectionProcessor* owner);

    /**
     * Method from request, may be overwritten by:
     * - X-Method-Override header.
     * - method_ URL param if ini config allows it.
     */
    const http::Method::ValueType& method() const;
    QString path() const;

    /**
     * If method may not provide message body, uses URL params.
     * If method provides message body, parses message body instead (url params may be merged into
     * it if ini config permits so).
     */
    const Params& params() const;

    /** Returns parsed param value, returns std::nullpot on failure. */
    template<typename T = QString>
    std::optional<T> param(const QString& key) const;

    /** The save as param, but returns defaultValue on failure. */
    template<typename T = QString>
    T paramOr(const QString& key, const T& defaultValue = {}) const;

    /** The save as param, but throws RestException on failure. */
    template<typename T = QString>
    T paramOrThrow(const QString& key) const;

    /**
     * If method may not provide message body, parses URL params.
     * If method provides message body, parses message body instead.
     */
    template <typename T>
    std::optional<T> parseContent() const;

    /** The save as parseContent, but throws RestException on failure. */
    template <typename T>
    T parseContentOrThrow() const;

    bool isExtraFormattingRequired() const;

private:
    http::Method::ValueType calculateMethod() const;
    std::optional<QJsonValue> parseMessageBody() const;
    Params calculateParams() const;
    std::optional<QJsonValue> calculateContent() const;

private:
    const Params m_urlParams;
    const http::Method::ValueType m_method;
    mutable std::optional<Params> m_paramsCache;
};

//--------------------------------------------------------------------------------------------------

template<typename T>
std::optional<T> Request::param(const QString& key) const
{
    const auto stringValue = param<QString>(key);
    if (!stringValue)
        return std::nullopt;

    T value;
    if (!QnLexical::deserialize(*stringValue, &value))
        return std::nullopt;

    return value;
}

template<>
inline std::optional<QString> Request::param<QString>(const QString& key) const
{
    const auto& params_ = params();
    const auto it = params_.find(key);
    if (it == params_.end())
        return std::nullopt;

    return it.value();
}

template<typename T>
T Request::paramOr(const QString& key, const T& defaultValue) const
{
    auto value = param<T>(key);
    return value ? *value : defaultValue;
}

template<typename T>
T Request::paramOrThrow(const QString& key) const
{
    const auto stringValue = paramOrThrow<QString>(key);
    T value;
    if (!QnLexical::deserialize(stringValue, &value))
        throw Exception(QnRestResult::InvalidParameter, key, stringValue);

    return value;
}

template<>
inline QString Request::paramOrThrow<QString>(const QString& key) const
{
    const auto stringValue = param<QString>(key);
    if (!stringValue)
        throw Exception(QnRestResult::MissingParameter, key);

    return *stringValue;
}

template<typename T>
std::optional<T> Request::parseContent() const
{
    const auto value = calculateContent();
    if (!value)
        return std::nullopt;

    T result;
    if (!QJson::deserialize(*value, &result))
        return std::nullopt;

    return result;
}

template <typename T>
T Request::parseContentOrThrow() const
{
    auto value = parseContent<T>();
    if (value)
        return *value;

    throw Exception(
        QnRestResult::CantProcessRequest,
        lm("Unable to parse request of %1").args(typeid(T)));
}

} // namespace nx::network::rest
