
#include "proxy_receiver_connection_processor.h"
#include "network/universal_request_processor_p.h"
#include "network/tcp_connection_priv.h"
#include "network/universal_tcp_listener.h"
#include <nx/network/http/custom_headers.h>
#include <nx/mediaserver/reverse_connection_manager.h>
#include <media_server/media_server_module.h>

QnProxyReceiverConnection::QnProxyReceiverConnection(
    QnMediaServerModule* serverModule,
    QSharedPointer<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
:
    nx::mediaserver::ServerModuleAware(serverModule),
    QnTCPConnectionProcessor(socket, owner)
{
    setObjectName(::toString(this));
}

void QnProxyReceiverConnection::run()
{
    Q_D(QnTCPConnectionProcessor);

    parseRequest();

    const QString mServerAddress = d->socket->getForeignAddress().address.toString();
    sendResponse(nx::network::http::StatusCode::ok, QByteArray());

    auto guid = nx::network::http::getHeaderValue(d->request.headers, Qn::PROXY_SENDER_HEADER_NAME);
    auto owner = static_cast<QnUniversalTcpListener*>(d->owner);
    auto manager = serverModule()->reverseConnectionManager();
    std::unique_ptr<nx::network::AbstractStreamSocket> socketPtr(d->socket.data());
    if (manager->registerProxyReceiverConnection(guid, std::move(socketPtr)))
        takeSocket();
}
