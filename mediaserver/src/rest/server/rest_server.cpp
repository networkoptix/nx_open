#include "rest_server.h"
#include "rest_connection_processor.h"

QnRestServer::QnRestServer(const QHostAddress& address, int port):
    QnTcpListener(address, port)
{

}

QnRestServer::~QnRestServer()
{
    stop();

    foreach(QnRestRequestHandler* handler, m_handlers)
    {
        delete handler;
    }
}

QnTCPConnectionProcessor* QnRestServer::createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner)
{
    return new QnRestConnectionProcessor(clientSocket, owner);
}

void QnRestServer::registerHandler(const QString& path, QnRestRequestHandler* handler)
{
    m_handlers.insert(path, handler);
    handler->setPath(path);
}

QnRestRequestHandler* QnRestServer::findHandler(QString path)
{
    if (path.startsWith('/'))
        path = path.mid(1);
	if (path.endsWith('/'))
		path = path.left(path.length()-1);

    for (Handlers::iterator i = m_handlers.begin();i != m_handlers.end(); ++i)
    {
        QRegExp expr(i.key(), Qt::CaseSensitive, QRegExp::Wildcard);
        if (expr.indexIn(path) != -1)
            return i.value();
    }

    return 0;
}

const QnRestServer::Handlers& QnRestServer::allHandlers() const
{
    return m_handlers;
}
