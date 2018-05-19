#include "reverse_connection_listener.h"

#include <network/tcp_connection_priv.h>
#include <nx/network/http/custom_headers.h>
#include "reverse_connection_manager.h"

namespace nx {
namespace vms {
namespace network {

ReverseConnectionListener::ReverseConnectionListener(
    ReverseConnectionManager* reverseConnectionManager,
    QSharedPointer<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
:
    m_reverseConnectionManager(reverseConnectionManager),
    QnTCPConnectionProcessor(socket, owner)
{
    setObjectName(::toString(this));
}

void ReverseConnectionListener::run()
{
    Q_D(QnTCPConnectionProcessor);

    parseRequest();

    const QString mServerAddress = d->socket->getForeignAddress().address.toString();
    sendResponse(nx::network::http::StatusCode::ok, QByteArray());

    auto guid = nx::network::http::getHeaderValue(d->request.headers, Qn::PROXY_SENDER_HEADER_NAME);
    std::unique_ptr<nx::network::AbstractStreamSocket> socketPtr(d->socket.data());
    if (m_reverseConnectionManager->addIncomingTcpConnection(guid, std::move(socketPtr)))
        takeSocket();
}

} // namespace network
} // namespace vms
} // namespace nx
