#include "proxy_listener.h"
#include "proxy_connection.h"

QnProxyListener::QnProxyListener(const QHostAddress& address, int port):
    QnTcpListener(address, port)
{

}

QnProxyListener::~QnProxyListener()
{
    stop();
}

QnTCPConnectionProcessor* QnProxyListener::createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, QnTcpListener* owner)
{
    return new QnProxyConnectionProcessor(clientSocket, owner);
}
