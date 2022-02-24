// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "incoming_tunnel_connection.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx::network::cloud::udp {

IncomingTunnelConnection::IncomingTunnelConnection(
    std::unique_ptr<IncomingControlConnection> controlConnection,
    std::unique_ptr<AbstractStreamServerSocket> serverSocket)
    :
    m_state(SystemError::noError),
    m_controlConnection(std::move(controlConnection)),
    m_serverSocket(std::move(serverSocket))
{
    if (!m_serverSocket)
        m_serverSocket = std::make_unique<UdtStreamServerSocket>(SocketFactory::udpIpVersion());

    bindToAioThread(getAioThread());

    auto controlSocket = m_controlConnection->socket();

    m_controlConnection->setErrorHandler(
        [this](SystemError::ErrorCode code)
        {
            m_controlConnection.reset();
            NX_DEBUG(this, nx::format("Control connection error (%1), closing tunnel...")
                .arg(SystemError::toString(code)));

            m_state = code;
            if (m_serverSocket)
                m_serverSocket->pleaseStopSync(); //< We are in AIO thread.

            if (m_acceptHandler)
                nx::utils::swapAndCall(m_acceptHandler, code, nullptr);
        });

    const SocketAddress addressToBind(
        HostAddress::anyHost,
        controlSocket->getLocalAddress().port);
    if (!m_serverSocket->setNonBlockingMode(true) ||
        !m_serverSocket->bind(addressToBind) ||
        !m_serverSocket->listen())
    {
        NX_WARNING(this, nx::format("Can not listen on server socket %1: %2")
            .args(addressToBind, SystemError::getLastOSErrorText()));

        m_state = SystemError::getLastOSErrorCode();
    }
    else
    {
        NX_DEBUG(this, nx::format("Listening for new connections on %1")
            .arg(m_serverSocket->getLocalAddress().toString()));
    }
}

void IncomingTunnelConnection::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_controlConnection->bindToAioThread(aioThread);
    m_serverSocket->bindToAioThread(aioThread);
}

void IncomingTunnelConnection::accept(AcceptHandler handler)
{
    NX_ASSERT(!m_acceptHandler, "Concurrent accept");
    m_serverSocket->post(
        [this, handler = std::move(handler)]() mutable
        {
            if (m_state != SystemError::noError)
                return handler(m_state, nullptr);

            m_acceptHandler = std::move(handler);
            m_serverSocket->acceptAsync(
                [this](
                    SystemError::ErrorCode code,
                    std::unique_ptr<AbstractStreamSocket> socket)
                {
                    NX_VERBOSE(this, nx::format("Accepted %1 (%2)")
                        .arg(socket).arg(SystemError::toString(code)));

                    if (code != SystemError::noError)
                        m_state = code;
                    else
                        m_controlConnection->resetLastKeepAlive();

                    nx::utils::swapAndCall(m_acceptHandler, code, std::move(socket));
                });
        });
}

void IncomingTunnelConnection::stopWhileInAioThread()
{
    m_controlConnection.reset();
    m_serverSocket.reset();
}

} // namespace nx::network::cloud::udp
