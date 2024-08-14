// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "url_builder.h"

#include <nx/utils/string.h>

namespace nx::network::url {

Builder::Builder(const nx::utils::Url& url):
    m_url(url)
{
}

nx::utils::Url Builder::toUrl() const
{
    return m_url;
}

Builder::operator utils::Url() const
{
    return toUrl();
}

Builder& Builder::setScheme(const QString& scheme)
{
    m_url.setScheme(scheme);
    return *this;
}

Builder& Builder::setAuthority(const std::string& authority, QUrl::ParsingMode mode)
{
    m_url.setAuthority(QString::fromStdString(authority), mode);
    return *this;
}

Builder& Builder::setUserInfo(const std::string& userInfo, QUrl::ParsingMode mode)
{
    m_url.setUserInfo(QString::fromStdString(userInfo), mode);
    return *this;
}

Builder& Builder::setUserName(const QString& userName, QUrl::ParsingMode mode)
{
    m_url.setUserName(userName, mode);
    return *this;
}

Builder& Builder::setPassword(const QString& password, QUrl::ParsingMode mode)
{
    m_url.setPassword(password, mode);
    return *this;
}

Builder& Builder::setHost(const QString& host, QUrl::ParsingMode mode)
{
    m_url.setHost(host, mode);
    setPath(m_url.path().toStdString());
    return *this;
}

Builder& Builder::setPort(int port)
{
    m_url.setPort(port);
    return *this;
}

Builder& Builder::setEndpoint(const SocketAddress& endpoint, QUrl::ParsingMode mode)
{
    setHost(endpoint.address.toString(), mode);
    if (endpoint.port > 0)
        m_url.setPort(endpoint.port);
    return *this;
}

Builder& Builder::setPath(const std::string_view& path, QUrl::ParsingMode mode)
{
    // QUrl does not like several slashes at the beginning, as well as no slash at all.
    auto actualPath = normalizePath(path);

    const bool shouldStartWithSlash = !m_url.host().isEmpty();

    if (shouldStartWithSlash)
    {
        if (!actualPath.empty() && !nx::utils::startsWith(actualPath, '/'))
            actualPath = '/' + actualPath;
    }
    else
    {
        while (nx::utils::startsWith(actualPath, '/'))
            actualPath = actualPath.substr(1);
    }

    m_url.setPath(actualPath, mode);
    return *this;
}

Builder& Builder::setPath(const QString& path, QUrl::ParsingMode mode)
{
    return setPath((std::string_view) path.toStdString(), mode);
}

Builder& Builder::appendPath(const std::string_view& path, QUrl::ParsingMode mode)
{
    if (path.empty())
        return *this;

    auto value = m_url.path().toStdString();
    if (!nx::utils::endsWith(value, '/') && !nx::utils::startsWith(path, '/'))
        value += '/';
    value += path;
    setPath(value, mode);
    return *this;
}

Builder& Builder::appendPath(const QString& path, QUrl::ParsingMode mode)
{
    return appendPath((std::string_view) path.toStdString(), mode);
}

Builder& Builder::setQuery(const QString& query, QUrl::ParsingMode mode)
{
    m_url.setQuery(query, mode);
    return *this;
}

Builder& Builder::setQuery(const QUrlQuery& query)
{
    m_url.setQuery(query);
    return *this;
}

Builder& Builder::setQuery(const nx::utils::UrlQuery& query)
{
    m_url.setQuery(query);
    return *this;
}

Builder& Builder::addQueryItem(const QString& name, const QString& value)
{
    // TODO: Optimize by QUrlQuery field in Builder.
    nx::utils::UrlQuery query(m_url.query());
    query.addQueryItem(name, value);
    return setQuery(query);
}

Builder& Builder::removeQueryItem(const QString& name)
{
    nx::utils::UrlQuery query(m_url.query());
    if (query.hasQueryItem(name))
    {
        query.removeQueryItem(name);
        m_url.setQuery(query);
    }
    return *this;
}

Builder& Builder::setFragment(const QString& fragment, QUrl::ParsingMode mode)
{
    m_url.setFragment(fragment, mode);
    return *this;
}

Builder& Builder::setPathAndQuery(const QString& pathAndQuery)
{
    const int queryPos = pathAndQuery.indexOf('?');
    if (queryPos == -1)
    {
        setPath(pathAndQuery);
    }
    else
    {
        setPath(pathAndQuery.left(queryPos));
        setQuery(pathAndQuery.mid(queryPos + 1));
    }
    return *this;
}

}// namespace nx::network::url
