#pragma once

#include <QtCore/QUrl>

#include "url_parse_helper.h"
#include "../socket_common.h"

namespace nx {
namespace network {
namespace url {

class Builder
{
public:
    Builder(const nx::utils::Url& url = nx::utils::Url()): m_url(url) {}

    nx::utils::Url toUrl() const { return m_url; }
    operator nx::utils::Url() const { return toUrl(); }

    QString toString(QUrl::FormattingOptions options =
        QUrl::FormattingOptions(QUrl::PrettyDecoded)) const
    {
        return toUrl().toString(options);
    }

    Builder& setScheme(const QString& scheme)
    {
        m_url.setScheme(scheme);
        return *this;
    }
    Builder& setAuthority(const QString& authority, QUrl::ParsingMode mode = QUrl::TolerantMode)
    {
        m_url.setAuthority(authority, mode);
        return *this;
    }
    Builder& setUserInfo(const QString& userInfo, QUrl::ParsingMode mode = QUrl::TolerantMode)
    {
        m_url.setUserInfo(userInfo, mode);
        return *this;
    }
    Builder& setUserName(const QString& userName, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setUserName(userName, mode);
        return *this;
    }
    Builder& setPassword(const QString& password, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setPassword(password, mode);
        return *this;
    }
    Builder& setHost(const QString& host, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setHost(host, mode);
        return *this;
    }
    Builder& setPort(int port)
    {
        m_url.setPort(port);
        return *this;
    }
    Builder& setEndpoint(const SocketAddress& endpoint, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setHost(endpoint.address.toString(), mode);
        if (endpoint.port > 0)
            m_url.setPort(endpoint.port);
        return *this;
    }
    Builder& setPath(const QString& path, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setPath(path, mode);
        return *this;
    }
    Builder& appendPath(const QString& path, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setPath(normalizePath(m_url.path() + path), mode);
        return *this;
    }
    Builder& setQuery(const QString& query, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setQuery(query, mode);
        return *this;
    }
    Builder& setQuery(const QUrlQuery& query)
    {
        m_url.setQuery(query);
        return *this;
    }
    Builder& setFragment(const QString& fragment, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setFragment(fragment, mode);
        return *this;
    }

private:
    nx::utils::Url m_url;
};

} // namespace url
} // namespace network
} // namespace nx
