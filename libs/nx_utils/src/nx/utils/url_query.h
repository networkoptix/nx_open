#pragma once

#include <QUrlQuery>

namespace nx::utils {

class NX_UTILS_API UrlQuery
{
public:
    UrlQuery& add(const QString& key, const QString& value);
    UrlQuery& add(const QString& key, const std::string& value);
    UrlQuery& add(const char* key, const char* value);

    template<
        typename NumericType,
        typename =
            typename std::enable_if<std::is_arithmetic<NumericType>::value, NumericType>::type>
    UrlQuery& add(const QString& key, NumericType value)
    {
        return add(key, QString::number(value));
    }

    QString toString(QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const;

    operator const QUrlQuery&() const;

private:
    QUrlQuery m_query;
};

} // namespace nx::utils
