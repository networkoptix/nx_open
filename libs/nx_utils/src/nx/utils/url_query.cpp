#include "url_query.h"

namespace nx::utils {

UrlQuery& UrlQuery::add(const QString& key, const QString& value)
{
    m_query.addQueryItem(key, value);
    return *this;
}

UrlQuery& UrlQuery::add(const QString& key, const std::string& value)
{
    return add(key, QString::fromStdString(value));
}

UrlQuery& UrlQuery::add(const char* key, const char* value)
{
    return add(QString(key), QString(value));
}

QString UrlQuery::toString(QUrl::ComponentFormattingOptions encoding) const
{
    return m_query.toString(encoding);
}

UrlQuery::operator const QUrlQuery&() const
{
    return m_query;
}

} // namespace nx::utils
