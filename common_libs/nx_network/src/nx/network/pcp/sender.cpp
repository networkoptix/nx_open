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
    m_socket->sendAsync(
        *request,
        [this, request = std::move(request)](SystemError::ErrorCode result, size_t)
    {
        if (result != SystemError::noError)
        {
            NX_LOGX(lm("Send error: %1").arg(SystemError::toString(result)),
                cl_logDEBUG1);

            return;
        }

        NX_LOGX(lm("Sent: %1").arg(request->toHex()), cl_logDEBUG2);
    });
}

} // namespace pcp
