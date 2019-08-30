#pragma once

#include <nx/utils/std/charconv.h>

#include <QUrlQuery>

namespace nx::utils {

/**
 * Simple wrapper around QUrlQUery with overloads for std::string and numeric types.
 * Simplifies creating queries when given, for example, a data structure where all the fields
 * are std::string or numeric types, by hiding calls to std::string::c_str() or
 * std::to_string(number)
 */
class NX_UTILS_API UrlQuery
{
public:
    UrlQuery() = default;
    UrlQuery(const QString& query);

    UrlQuery& addQueryItem(const QString& key, const QString& value);
    UrlQuery& addQueryItem(const QString& key, const std::string& value);
    UrlQuery& addQueryItem(const char* key, const char* value);

    template<
        typename NumericType,
        typename = typename std::enable_if_t<std::is_arithmetic_v<NumericType>, NumericType>>
    // requires std::is_arithmetic_v<NumericType>
    UrlQuery& addQueryItem(const QString& key, NumericType value)
    {
        return addQueryItem(key, QString::number(value));
    }

    bool hasQueryItem(const QString& key) const;

    QString queryItemValue(
        const QString& key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const;

    QString queryItemValue(
        const std::string& key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const;

    QString queryItemValue(
        const char* key,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const;

    QString toString(QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const;

    operator const QUrlQuery&() const;

private:
    QUrlQuery m_query;
};

} // namespace nx::utils
