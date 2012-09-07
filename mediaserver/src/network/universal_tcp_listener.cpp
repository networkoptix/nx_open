#include "universal_tcp_listener.h"
#include "utils/network/tcp_connection_priv.h"

class QnUniversalRequestProcessor: public QnTCPConnectionProcessor
{
    QN_DECLARE_PRIVATE_DERIVED(QnUniversalRequestProcessor);
public:
    QnUniversalRequestProcessor(TCPSocket* socket, QnTcpListener* owner);
protected:
    virtual void run() override;
    virtual void pleaseStop() override;
};

struct QnUniversalRequestProcessor::QnUniversalRequestProcessorPrivate: public QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
    QnTCPConnectionProcessor* processor;
    QMutex mutex;
};

QnUniversalRequestProcessor::QnUniversalRequestProcessor(TCPSocket* socket, QnTcpListener* owner):
    QnTCPConnectionProcessor(new QnUniversalRequestProcessorPrivate, socket, owner)
{
    Q_D(QnUniversalRequestProcessor);
    d->processor = 0;
}

void QnUniversalRequestProcessor::run()
{
    Q_D(QnUniversalRequestProcessor);
    if (readRequest()) 
    {
        QList<QByteArray> header = d->clientRequest.left(d->clientRequest.indexOf('\n')).split(' ');
        if (header.size() > 2) 
        {
            QByteArray protocol = header[2].split('/')[0].toUpper();
            QMutexLocker lock(&d->mutex);
            d->processor = dynamic_cast<QnUniversalTcpListener*>(d->owner)->createNativeProcessor(d->socket, protocol, QUrl(header[1]).path());
            if (d->processor && !m_needStop) 
            {
                copyClientRequestTo(*d->processor);
                d->ssl = 0;
                d->socket = 0;
                d->processor->execute(d->mutex);
            }
            delete d->processor;
        }
    }
}

void QnUniversalRequestProcessor::pleaseStop()
{
    Q_D(QnUniversalRequestProcessor);
    QMutexLocker lock(&d->mutex);
    QnTCPConnectionProcessor::pleaseStop();
    if (d->processor)
        d->processor->pleaseStop();
}

// -------------------------------- QnUniversalListener ---------------------------------

QnUniversalTcpListener::QnUniversalTcpListener(const QHostAddress& address, int port):
    QnTcpListener(address, port)
{

}

QnUniversalTcpListener::~QnUniversalTcpListener()
{
    stop();
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createNativeProcessor(TCPSocket* clientSocket, const QByteArray& protocol, const QString& path)
{
    QString normPath = path.startsWith('/') ? path.mid(1) : path;
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        if (m_handlers[i].protocol == protocol && normPath.startsWith(m_handlers[i].path))
            return m_handlers[i].instanceFunc(clientSocket, this);
    }

    // check default '*' path handler
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        if (m_handlers[i].protocol == protocol && m_handlers[i].path == QString("*"))
            return m_handlers[i].instanceFunc(clientSocket, this);
    }

    return 0;
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner)
{
    return new QnUniversalRequestProcessor(clientSocket, owner);
}
