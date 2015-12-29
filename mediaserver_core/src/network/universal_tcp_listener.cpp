
#include "universal_tcp_listener.h"

#include <common/common_module.h>
#include <utils/common/log.h>

#include "universal_request_processor.h"
#include "proxy_sender_connection_processor.h"


QnUniversalTcpListener::QnUniversalTcpListener(
    const QHostAddress& address,
    int port,
    int maxConnections,
    bool useSsl)
:
    QnHttpConnectionListener(
        address,
        port,
        maxConnections,
        useSsl)
{
}

void QnUniversalTcpListener::addProxySenderConnections(const SocketAddress& proxyUrl, int size)
{
    if (m_needStop)
        return;

    NX_LOG(lit("QnHttpConnectionListener: %1 reverse connection(s) to %2 is(are) needed")
        .arg(size).arg(proxyUrl.toString()), cl_logDEBUG1);

    for (int i = 0; i < size; ++i)
    {
        auto connect = new QnProxySenderConnection(proxyUrl, qnCommon->moduleGUID(), this);
        connect->start();
        addOwnership(connect);
    }
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket)
{
    return new QnUniversalRequestProcessor(clientSocket, this, needAuth());
}
