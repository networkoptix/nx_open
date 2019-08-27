#include "url_query.h"

namespace nx::utils {

namespace detail {

bool convert(const QString& value, QString* outObject)
{
    *outObject = value;
    return true;
}

bool convert(const QString& value, std::string* outObject)
{
    *outObject = value.toStdString();
    return true;
}

bool convert(const QString& value, int* outObject)
{
    bool ok = false;
    *outObject = value.toInt(&ok);
    return ok;
}

} // namespace detail

UrlQuery::UrlQuery(const QString& query):
    m_query(query)
{
}

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

bool UrlQuery::hasKey(const QString& key) const
{
    return m_query.hasQueryItem(key);
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
