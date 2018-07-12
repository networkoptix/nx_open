#include "sender.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace pcp {

Sender::Sender(const HostAddress& server)
    : m_socket(SocketFactory::createDatagramSocket())
{
    // TODO: Verify return codes.
    m_socket->setDestAddr(SocketAddress(server, SERVER_PORT));
    m_socket->setNonBlockingMode(true);
}

Sender::~Sender()
{
    m_socket->pleaseStopSync();
}

void Sender::send(std::shared_ptr<QByteArray> request)
{
    auto& buffer = *request;
    m_socket->sendAsync(
        buffer,
        [this, request = std::move(request)](SystemError::ErrorCode result, size_t size)
        {
            if (result != SystemError::noError || size != (size_t)request->size())
            {
                NX_DEBUG(this, lm("PCP Send (size=%1) error: %2")
                    .arg(size).arg(SystemError::toString(result)));
            }
            else
            {
                NX_VERBOSE(this, lm("PCP Sent (size=%1): %2").arg(size).arg(request->toHex()));
            }
        });
}

} // namespace pcp
} // namespace network
} // namespace nx
