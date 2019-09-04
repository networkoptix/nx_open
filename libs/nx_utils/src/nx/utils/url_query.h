#pragma once

#include <QUrlQuery>

#include <nx/utils/std/charconv.h>

namespace nx::utils {

namespace detail {

void NX_UTILS_API convert(const QString& value, std::string* outValue);
void NX_UTILS_API convert(const QString& value, QString* outValue);

} // namespace detail

/**
 * Simple wrapper around QUrlQUery with overloads for std::string and numeric types.
 * Simplifies creating queries when given, for example, a data structure where all the fields
 * are std::string or numeric types, by hiding calls to std::string::c_str() or
 * std::to_string(number)
 */
class NX_UTILS_API UrlQuery
{
public:
    UrlQuery(const QString& query = QString());

    UrlQuery& addQueryItem(const QString& key, const QString& value);
    UrlQuery& addQueryItem(const QString& key, const std::string& value);
    UrlQuery& addQueryItem(const char* key, const char* value);

    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>, T>>
    UrlQuery& addQueryItem(const QString& key, T value)
    {
        return addQueryItem(key, QString::number(value));
    }

    bool hasQueryItem(const QString& key) const;

    template<typename T, typename = std::enable_if_t<!std::is_arithmetic_v<T>, T>>
    T queryItemValue(
        const QString& key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const
    {
        T value;
        detail::convert(m_query.queryItemValue(key, encoding), &value);
        return value;
    }

    template<typename T, typename = std::enable_if_t<!std::is_arithmetic_v<T>, T>>
    T queryItemValue(
        const std::string& key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const
    {
        return queryItemValue<T>(QString::fromStdString(key), encoding);
    }

    template<typename T, typename = std::enable_if_t<!std::is_arithmetic_v<T>, T>>
    T queryItemValue(
        const char* key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const
    {
        return queryItemValue<T>(QString(key), encoding);
    }

    template<typename T, typename = std::enable_if_t<std::is_integral_v<T>, T>>
    // requires std::is_integral_v<T>
    T queryItemValue(
        const QString& key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded,
        bool* ok = nullptr,
        int base = 10) const
    {
        T value;
        const auto str = m_query.queryItemValue(key, encoding).toStdString();
        const auto result =
            charconv::from_chars(&str.front(), &str.front() + str.size(), value, base);
        if (ok)
            *ok = result.ec == std::errc();
        return value;
    }

    template<typename T, typename = std::enable_if_t<std::is_integral_v<T>, T>>
    // requires std::is_integral_v<T>
    T queryItemValue(
        const std::string& key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded,
        bool* ok = nullptr,
        int base = 10) const
    {
        return queryItemValue<T>(QString::fromStdString(key), encoding, ok, base);
    }

    template<typename T, typename = std::enable_if_t<std::is_integral_v<T>, T>>
    // requires std::is_integral_v<T>
    T queryItemValue(
        const char* key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded,
        bool* ok = nullptr,
        int base = 10) const
    {
        return queryItemValue<T>(QString(key), encoding, ok, base);
    }

    template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>, T>>
    // requires std::is_floating_point_v<T>
    T queryItemValue(
        const QString& key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded,
        bool* ok = nullptr,
        charconv::chars_format format = charconv::chars_format::general) const
    {
        T value;
        const auto str = m_query.queryItemValue(key, encoding).toStdString();
        const auto result =
            charconv::from_chars(&str.front(), &str.front() + str.size(), value, format);
        if (ok)
            *ok = result.ec == std::errc();
        return value;
    }

    template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>, T>>
    // requires std::is_floating_point_v<T>
    T queryItemValue(
        const std::string& key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded,
        bool* ok = nullptr,
        charconv::chars_format format = charconv::chars_format::general) const
    {
        return queryItemValue<T>(QString::fromStdString(key), encoding, ok, format);
    }

    template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>, T>>
    // requires std::is_floating_point_v<T>
    T queryItemValue(
        const char* key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded,
        bool* ok = nullptr,
        charconv::chars_format format = charconv::chars_format::general) const
    {
        return queryItemValue<T>(QString(key), encoding, ok, format);
    }

    QString toString(QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const;

    operator const QUrlQuery&() const;

private:
    QUrlQuery m_query;
};

} // namespace nx::utils
