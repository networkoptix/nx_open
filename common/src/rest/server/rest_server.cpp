#include "rest_server.h"
#include "rest_connection_processor.h"

QnRestServer::QnRestServer(const QHostAddress& address, int port):
    QnTcpListener(address, port)
{

}

QnRestServer::~QnRestServer()
{
    stop();
}

QnTCPConnectionProcessor* QnRestServer::createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, QnTcpListener* owner)
{
    return new QnRestConnectionProcessor(clientSocket, owner);
}
