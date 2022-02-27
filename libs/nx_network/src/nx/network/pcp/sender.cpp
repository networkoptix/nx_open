// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sender.h"

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

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

void Sender::send(std::shared_ptr<nx::Buffer> request)
{
    m_socket->sendAsync(
        request.get(),
        [this, request](SystemError::ErrorCode result, size_t size)
        {
            if (result != SystemError::noError || size != (size_t)request->size())
            {
                NX_DEBUG(this, nx::format("PCP Send (size=%1) error: %2")
                    .arg(size).arg(SystemError::toString(result)));
            }
            else
            {
                NX_VERBOSE(this, nx::format("PCP Sent (size=%1): %2").arg(size).arg(nx::utils::toHex(*request)));
            }
        });
}

} // namespace pcp
} // namespace network
} // namespace nx
