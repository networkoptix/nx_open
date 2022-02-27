// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>

#include <QUrlQuery>

#include <nx/utils/log/to_string.h>
#include <nx/utils/std/charconv.h>

namespace nx::utils {

namespace detail {

NX_UTILS_API void convert(QString&& value, std::string* outValue);
NX_UTILS_API void convert(QString&& value, QString* outValue);

} // namespace detail

/**
 * Simple wrapper around QUrlQUery with support for STL string types and numeric types.
 * Simplifies creating queries when given, for example, a data structure where all the fields
 * are std::string or numeric types, by hiding calls to std::string::c_str() or
 * std::to_string(number).
 */
class NX_UTILS_API UrlQuery
{
public:
    UrlQuery(const QString& query = QString());
    explicit UrlQuery(const QUrlQuery& query): m_query(query){};

    template<
        typename String, typename T,
        typename = std::enable_if_t<!std::is_arithmetic_v<std::decay_t<T>>>
    >
    UrlQuery& addQueryItem(String&& key, T&& value)
    {
        m_query.addQueryItem(
            nx::toString(std::forward<String>(key)),
            nx::toString(std::forward<T>(value)));
        return *this;
    }

    /** Add a numeric item. Note that bool is represented with 1/0 (true/false). */
    template<
        typename String, typename T,
        typename = std::enable_if_t<std::is_arithmetic_v<std::decay_t<T>>>
    >
    UrlQuery& addQueryItem(String&& key, T value)
    {
        m_query.addQueryItem(
            nx::toString(std::forward<String>(key)),
            QString::number(value));
        return *this;
    }

    bool hasQueryItem(const QString& key) const;

    bool contains(const std::string_view& key) const;

    template<
        typename T, typename String,
        typename = std::enable_if_t<!std::is_arithmetic_v<T>>
    >
    // requires !std::is_arithmetic_v<T>
    T queryItemValue(
        const String& key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const
    {
        T value;
        detail::convert(m_query.queryItemValue(prepareKey(key), encoding), &value);
        return value;
    }

    template<
        typename T, typename String,
        typename = std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>
    >
    // requires std::is_integral_v<T> && !std::is_same_v<T, bool>
    T queryItemValue(
        const String& key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded,
        bool* ok = nullptr,
        int base = 10) const
    {
        T value;
        const auto str = m_query.queryItemValue(prepareKey(key), encoding).toStdString();
        const auto result =
            charconv::from_chars(&str.front(), &str.front() + str.size(), value, base);
        if (ok)
            *ok = result.ec == std::errc();
        return value;
    }

    template<
        typename T, typename String,
        typename = std::enable_if_t<std::is_floating_point_v<T>>
    >
    // requires std::is_floating_point_v<T>
    T queryItemValue(
        const String& key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded,
        bool* ok = nullptr,
        charconv::chars_format format = charconv::chars_format::general) const
    {
        T value;
        const auto str = m_query.queryItemValue(prepareKey(key), encoding).toStdString();
        const auto result =
            charconv::from_chars(&str.front(), &str.front() + str.size(), value, format);
        if (ok)
            *ok = result.ec == std::errc();
        return value;
    }

    /**
     * Returns the item value as bool.
     */
    template<
        typename T,
        typename String,
        typename = std::enable_if_t<std::is_same_v<T, bool>>
    >
    // requires std::is_same_v<T, bool>
    T queryItemValue(
        const String& key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded,
        bool* ok = nullptr) const
    {
        int value = 0;
        const auto str = m_query.queryItemValue(prepareKey(key), encoding).toStdString();
        const auto result =
            charconv::from_chars(&str.front(), &str.front() + str.size(), value, 10);
        if (ok)
            *ok = result.ec == std::errc();
        return value != 0;
    }

    template<typename String>
    void removeQueryItem(const String& key)
    {
        m_query.removeQueryItem(prepareKey(key));
    }

    QString toString(QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const;

    operator const QUrlQuery&() const;

private:
    template<
        typename String,
        typename = std::enable_if_t<std::is_same_v<std::decay_t<String>, QString>>
    >
    static const QString& prepareKey(const String& str)
    {
        return str;
    }

    template<
        typename String,
        typename = std::enable_if_t<!std::is_same_v<std::decay_t<String>, QString>>
    >
    static QString prepareKey(const String& str)
    {
        return nx::toString(str);
    }

private:
    QUrlQuery m_query;
};

} // namespace nx::utils
