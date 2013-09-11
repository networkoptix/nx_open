#include "universal_request_processor.h"

#include <QList>
#include <QByteArray>
#include "utils/network/tcp_connection_priv.h"
#include "universal_tcp_listener.h"
#include "universal_request_processor_p.h"

QnUniversalRequestProcessor::~QnUniversalRequestProcessor()
{
    stop();
}

QnUniversalRequestProcessor::QnUniversalRequestProcessor(AbstractStreamSocket* socket, QnTcpListener* owner):
QnTCPConnectionProcessor(new QnUniversalRequestProcessorPrivate, socket)
{
    Q_D(QnUniversalRequestProcessor);
    d->processor = 0;
    d->owner = owner;

    setObjectName( QLatin1String("QnUniversalRequestProcessor") );
}

QnUniversalRequestProcessor::QnUniversalRequestProcessor(QnUniversalRequestProcessorPrivate* priv, AbstractStreamSocket* socket, QnTcpListener* owner):
QnTCPConnectionProcessor(priv, socket)
{
    Q_D(QnUniversalRequestProcessor);
    d->processor = 0;
    d->owner = owner;
}

void QnUniversalRequestProcessor::run()
{
    saveSysThreadID();
    if (readRequest()) 
        processRequest();
}

void QnUniversalRequestProcessor::processRequest()
{
    Q_D(QnUniversalRequestProcessor);
    QList<QByteArray> header = d->clientRequest.left(d->clientRequest.indexOf('\n')).split(' ');
    if (header.size() > 2) 
    {
        QByteArray protocol = header[2].split('/')[0].toUpper();
        QMutexLocker lock(&d->mutex);
        d->processor = dynamic_cast<QnUniversalTcpListener*>(d->owner)->createNativeProcessor(d->socket, protocol, QUrl(QString::fromUtf8(header[1])).path());
        if (d->processor && !needToStop()) 
        {
            copyClientRequestTo(*d->processor);
            d->socket = 0;
            d->processor->execute(d->mutex);
        }
        delete d->processor;
        d->processor = 0;
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
