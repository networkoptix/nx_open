#include "sender.h"

#include <nx/utils/log/log.h>

namespace pcp {

Sender::Sender(const HostAddress& server)
    : m_socket(SocketFactory::createDatagramSocket())
{
    m_socket->setDestAddr(SocketAddress(server, SERVER_PORT));
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
        if (result != SystemError::noError || size != request->size())
        {
            NX_LOG(lm("PCP Send (size=%1) error: %2")
                .arg(size).arg(SystemError::toString(result)), cl_logDEBUG1);
        }
        else
        {
            NX_LOG(lm("PCP Sent (size=%1): %2")
                .arg(size).arg(request->toHex()), cl_logDEBUG2);
        }
    });
}

} // namespace pcp
