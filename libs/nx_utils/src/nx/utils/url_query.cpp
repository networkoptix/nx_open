// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "url_query.h"

namespace nx::utils {

namespace detail {

void convert(QString&& value, std::string* outValue)
{
    *outValue = value.toStdString();
}

void convert(QString&& value, QString* outValue)
{
    *outValue = std::move(value);
}

} // namespace detail

UrlQuery::UrlQuery(const QString& query):
    m_query(query)
{
}

UrlQuery::UrlQuery(const std::string_view& str):
    m_query(QString::fromUtf8(str.data(), str.size()))
{
}

UrlQuery::UrlQuery(const char* str):
    m_query(QString::fromUtf8(str))
{
}

bool UrlQuery::hasQueryItem(const QString& key) const
{
    return m_query.hasQueryItem(key);
}

bool UrlQuery::contains(const std::string_view& key) const
{
    return hasQueryItem(QString::fromUtf8(key.data(), key.size()));
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
