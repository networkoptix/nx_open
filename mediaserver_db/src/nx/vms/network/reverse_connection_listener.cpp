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
    QnTCPConnectionProcessor(socket, owner),
    m_reverseConnectionManager(reverseConnectionManager)
{
    setObjectName(::toString(this));
}

void ReverseConnectionListener::run()
{
    Q_D(QnTCPConnectionProcessor);

    parseRequest();
    sendResponse(nx::network::http::StatusCode::ok, QByteArray());

    auto guid = nx::network::http::getHeaderValue(d->request.headers, Qn::PROXY_SENDER_HEADER_NAME);
    auto socket = std::make_unique<ShareSocketDelegate>(ShareSocketDelegate(takeSocket()));
    m_reverseConnectionManager->addIncomingTcpConnection(guid, std::move(socket));
}

} // namespace network
} // namespace vms
} // namespace nx
