
#include "proxy_receiver_connection_processor.h"
#include "network/universal_request_processor_p.h"
#include "network/tcp_connection_priv.h"
#include "network/universal_tcp_listener.h"
#include <nx/network/http/custom_headers.h>

QnProxyReceiverConnection::QnProxyReceiverConnection(
    QSharedPointer<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
:
    QnTCPConnectionProcessor(socket, owner)
{
    setObjectName(::toString(this));
}

//static bool isLocalAddress(const QString& addr)
//{
//    return addr == lit("localhost") || addr == lit("127.0.0.1");
//}

void QnProxyReceiverConnection::run()
{
    Q_D(QnTCPConnectionProcessor);

    parseRequest();

    QString mServerAddress = d->socket->getForeignAddress().address.toString();
    /*
    QnProxyMessageProcessor* processor = dynamic_cast<QnProxyMessageProcessor*> (QnProxyMessageProcessor::instance());
    if (!processor->isMediaServerAddr(QHostAddress(mServerAddress)) && ! isLocalAddress(mServerAddress)) {
        d->socket->close();
        return;
    }
    */

    sendResponse(nx::network::http::StatusCode::ok, QByteArray());

    auto guid = nx::network::http::getHeaderValue(d->request.headers, Qn::PROXY_SENDER_HEADER_NAME);
    auto owner = static_cast<QnUniversalTcpListener*>(d->owner);
    if (owner->registerProxyReceiverConnection(guid, d->socket))
        takeSocket();
}
