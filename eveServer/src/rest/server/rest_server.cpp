#include "rest_server.h"
#include "rest_connection_processor.h"

QnRestServer::QnRestServer(const QHostAddress& address, int port):
    QnTcpListener(address, port)
{

}

QnRestServer::~QnRestServer()
{
    foreach(QnRestRequestHandler* handler, m_handlers)
    {
        delete handler;
    }
}

CLLongRunnable* QnRestServer::createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner)
{
    return new QnRestConnectionProcessor(clientSocket, owner);
}

void QnRestServer::registerHandler(const QString& path, QnRestRequestHandler* handler)
{
    m_handlers.insert(path, handler);
}

QnRestRequestHandler* QnRestServer::findHandler(QString path)
{
    if (path.startsWith('/'))
        path = path.mid(1);
    Handlers::iterator i = m_handlers.find(path);
    return i != m_handlers.end() ? i.value() : 0;
}

const QnRestServer::Handlers& QnRestServer::allHandlers() const
{
    return m_handlers;
}
