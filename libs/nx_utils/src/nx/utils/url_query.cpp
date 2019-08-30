#include "url_query.h"

namespace nx::utils {

UrlQuery::UrlQuery(const QString& query):
    m_query(query)
{
}

UrlQuery& UrlQuery::addQueryItem(const QString& key, const QString& value)
{
    m_query.addQueryItem(key, value);
    return *this;
}

UrlQuery& UrlQuery::addQueryItem(const QString& key, const std::string& value)
{
    return addQueryItem(key, QString::fromStdString(value));
}

UrlQuery& UrlQuery::addQueryItem(const char* key, const char* value)
{
    return addQueryItem(QString(key), QString(value));
}

bool UrlQuery::hasQueryItem(const QString& key) const
{
    return m_query.hasQueryItem(key);
}

QString UrlQuery::queryItemValue(
    const QString& key,
    QUrl::ComponentFormattingOptions encoding) const
{
    return m_query.queryItemValue(key, encoding);
}

QString UrlQuery::queryItemValue(const std::string& key, QUrl::ComponentFormattingOptions encoding) const
{
    return queryItemValue(QString::fromStdString(key), encoding);
}

QString UrlQuery::queryItemValue(const char* key, QUrl::ComponentFormattingOptions encoding) const
{
    return queryItemValue(QString(key), encoding);
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
