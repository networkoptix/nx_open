#include "http_connection_listener.h"

#include <QtCore/QUrl>

#include <nx/utils/log/log.h>
#include <nx/network/socket.h>
#include <network/tcp_connection_priv.h>
#include <common/common_module.h>

QnHttpConnectionListener::QnHttpConnectionListener(
    QnCommonModule* commonModule,
    const QHostAddress& address,
    int port,
    int maxConnections,
    bool useSsl)
    :
    QnTcpListener(commonModule, address, port, maxConnections, useSsl),
    m_needAuth(true)
{
}

QnHttpConnectionListener::~QnHttpConnectionListener()
{
    stop();
}

bool QnHttpConnectionListener::isProxy(const nx::network::http::Request& request)
{
    return m_proxyInfo.proxyHandler && m_proxyInfo.proxyCond(commonModule(), request);
}

bool QnHttpConnectionListener::needAuth() const
{
    return m_needAuth;
}

QnHttpConnectionListener::InstanceFunc QnHttpConnectionListener::findHandler(
    const QByteArray& protocol, const nx::network::http::Request& request)
{
    if (isProxy(request))
        return m_proxyInfo.proxyHandler;

    QString normPath = normalizedPath(request.requestLine.url.path());

    int bestPathLen = -1;
    int bestIdx = -1;
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        HandlerInfo h = m_handlers[i];
        if ((m_handlers[i].protocol == "*" || m_handlers[i].protocol == protocol)
            && normPath.startsWith(m_handlers[i].path))
        {
            int pathLen = m_handlers[i].path.length();
            if (pathLen > bestPathLen)
            {
                bestIdx = i;
                bestPathLen = pathLen;
            }
        }
    }
    if (bestIdx >= 0)
        return m_handlers[bestIdx].instanceFunc;

    // Check default "*" path handler.
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        if (m_handlers[i].protocol == protocol && m_handlers[i].path == QLatin1String("*"))
            return m_handlers[i].instanceFunc;
    }

    // Check default "*' path and protocol handler.
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        if (m_handlers[i].protocol == "*" && m_handlers[i].path == QLatin1String("*"))
            return m_handlers[i].instanceFunc;
    }

    return 0;
}

void QnHttpConnectionListener::disableAuth()
{
    m_needAuth = false;
}

void QnHttpConnectionListener::doAddHandler(
    const QByteArray& protocol, const QString& path, InstanceFunc instanceFunc)
{
    HandlerInfo handler;
    handler.protocol = protocol;
    handler.path = path;
    handler.instanceFunc = instanceFunc;
    m_handlers.append(handler);
}
