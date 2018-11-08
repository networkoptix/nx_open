#include "reverse_connection_listener.h"

#include <network/tcp_connection_priv.h>
#include <nx/network/http/custom_headers.h>
#include "reverse_connection_manager.h"

namespace nx {
namespace vms {
namespace network {

ReverseConnectionListener::ReverseConnectionListener(
    ReverseConnectionManager* reverseConnectionManager,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
:
    QnTCPConnectionProcessor(std::move(socket), owner),
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
    m_reverseConnectionManager->addIncomingTcpConnection(guid, takeSocket());
}

} // namespace network
} // namespace vms
} // namespace nx
