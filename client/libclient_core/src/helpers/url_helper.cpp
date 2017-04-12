#include "url_helper.h"

QnUrlHelper::QnUrlHelper(const QUrl& url):
    m_url(url)
{
}

QUrl QnUrlHelper::url() const
{
    return m_url;
}

QString QnUrlHelper::scheme() const
{
    return m_url.scheme();
}

QString QnUrlHelper::host() const
{
    return m_url.host();
}

int QnUrlHelper::port() const
{
    return m_url.port();
}

QString QnUrlHelper::address() const
{
    auto result = m_url.host();
    const auto port = m_url.port();
    if (port > 0)
    {
        result += L':';
        result += QString::number(port);
    }
    return result;
}

QString QnUrlHelper::userName() const
{
    return m_url.userName();
}

QString QnUrlHelper::password() const
{
    return m_url.password();
}

QString QnUrlHelper::path() const
{
    return m_url.path();
}

QUrl QnUrlHelper::cleanUrl() const
{
    QUrl url;
    url.setScheme(m_url.scheme());
    url.setHost(m_url.host());
    url.setPort(m_url.port());
    return url;
}
