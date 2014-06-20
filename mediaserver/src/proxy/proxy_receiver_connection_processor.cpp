
#include "proxy_receiver_connection_processor.h"
#include "network/universal_request_processor_p.h"
#include "utils/network/tcp_connection_priv.h"
#include "network/universal_tcp_listener.h"

class QnProxyReceiverConnectionPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnTcpListener* owner;
    bool takeSocketOwnership;
};

QnProxyReceiverConnection::QnProxyReceiverConnection(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner):
    QnTCPConnectionProcessor(new QnProxyReceiverConnectionPrivate, socket)
{
    Q_D(QnProxyReceiverConnection);

    d->owner = owner;
    d->takeSocketOwnership = false;
    setObjectName( lit("QnProxyReceiverConnection") );
}

//static bool isLocalAddress(const QString& addr)
//{
//    return addr == lit("localhost") || addr == lit("127.0.0.1");
//}

void QnProxyReceiverConnection::run()
{
    Q_D(QnProxyReceiverConnection);

    parseRequest();

    if (d->protocol != "PROXY")
    {
        d->socket->close();
        return;
    }

    QString mServerAddress = d->socket->getForeignAddress().address.toString();
    /*
    QnProxyMessageProcessor* processor = dynamic_cast<QnProxyMessageProcessor*> (QnProxyMessageProcessor::instance());
    if (!processor->isMediaServerAddr(QHostAddress(mServerAddress)) && ! isLocalAddress(mServerAddress)) {
        d->socket->close();
        return;
    }
    */


    sendResponse(nx_http::StatusCode::ok, QByteArray());
    QString guid = d->request.requestLine.url.path();
    if ((dynamic_cast<QnUniversalTcpListener*>(d->owner))->registerProxyReceiverConnection(guid, d->socket)) {
        d->takeSocketOwnership = true; // remove ownership from socket
        d->socket.clear();
    }
}

bool QnProxyReceiverConnection::isTakeSockOwnership() const
{
    Q_D(const QnProxyReceiverConnection);
    return d->takeSocketOwnership;
}
