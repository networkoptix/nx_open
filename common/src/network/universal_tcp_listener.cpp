#include "universal_tcp_listener.h"
#include "utils/network/tcp_connection_priv.h"
#include <QUrl>

class QnUniversalRequestProcessorPrivate;

class QnUniversalRequestProcessor: public QnTCPConnectionProcessor
{
public:
    QnUniversalRequestProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnUniversalRequestProcessor();

protected:
    virtual void run() override;
    virtual void pleaseStop() override;

private:
    Q_DECLARE_PRIVATE(QnUniversalRequestProcessor);
};

class QnUniversalRequestProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnTCPConnectionProcessor* processor;
    QMutex mutex;
};

QnUniversalRequestProcessor::~QnUniversalRequestProcessor()
{
    stop();
}

QnUniversalRequestProcessor::QnUniversalRequestProcessor(TCPSocket* socket, QnTcpListener* owner):
    QnTCPConnectionProcessor(new QnUniversalRequestProcessorPrivate, socket, owner)
{
    Q_D(QnUniversalRequestProcessor);
    d->processor = 0;

    setObjectName( QLatin1String("QnUniversalRequestProcessor") );
}

void QnUniversalRequestProcessor::run()
{
    Q_D(QnUniversalRequestProcessor);
    saveSysThreadID();
    if (readRequest()) 
    {
        QList<QByteArray> header = d->clientRequest.left(d->clientRequest.indexOf('\n')).split(' ');
        if (header.size() > 2) 
        {
            QByteArray protocol = header[2].split('/')[0].toUpper();
            QMutexLocker lock(&d->mutex);
            d->processor = dynamic_cast<QnUniversalTcpListener*>(d->owner)->createNativeProcessor(d->socket, protocol, QUrl(QString::fromUtf8(header[1])).path());
            if (d->processor && !needToStop()) 
            {
                copyClientRequestTo(*d->processor);
                d->ssl = 0;
                d->socket = 0;
                d->processor->execute(d->mutex);
            }
            delete d->processor;
            d->processor = 0;
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

QnUniversalTcpListener::QnUniversalTcpListener(const QHostAddress& address, int port, int maxConnections):
    QnTcpListener(address, port, maxConnections)
{

}

QnUniversalTcpListener::~QnUniversalTcpListener()
{
    stop();
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createNativeProcessor(TCPSocket* clientSocket, const QByteArray& protocol, const QString& path)
{
    QString normPath = path.startsWith(L'/') ? path.mid(1) : path;
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        HandlerInfo h = m_handlers[i];
        if (m_handlers[i].protocol == protocol && normPath.startsWith(m_handlers[i].path))
            return m_handlers[i].instanceFunc(clientSocket, this);
    }

    // check default '*' path handler
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        if (m_handlers[i].protocol == protocol && m_handlers[i].path == QLatin1String("*"))
            return m_handlers[i].instanceFunc(clientSocket, this);
    }

    // check default '*' path and protocol handler
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        if (m_handlers[i].protocol == "*" && m_handlers[i].path == QLatin1String("*"))
            return m_handlers[i].instanceFunc(clientSocket, this);
    }

    return 0;
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner)
{
    return new QnUniversalRequestProcessor(clientSocket, owner);
}
