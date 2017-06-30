#include "tunnel_tcp_available_endpoint_verificator.h"

#include <nx/network/socket_factory.h>
#include <nx/network/system_socket.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

AvailableEndpointVerificator::AvailableEndpointVerificator(
    const nx::String& connectSessionId)
    :
    m_connectSessionId(connectSessionId)
{
}

void AvailableEndpointVerificator::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    if (m_connection)
        m_connection->bindToAioThread(aioThread);
}

void AvailableEndpointVerificator::setTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

void AvailableEndpointVerificator::verifyHost(
    const SocketAddress& endpointToVerify,
    const AddressEntry& /*desiredHostAddress*/,
    nx::utils::MoveOnlyFunc<void(VerificationResult)> completionHandler)
{
    using namespace std::placeholders;

    m_completionHandler = std::move(completionHandler);

    m_connection = std::make_unique<TCPSocket>(SocketFactory::tcpClientIpVersion());
    m_connection->bindToAioThread(getAioThread());
    m_connection->setNonBlockingMode(true);
    if (m_timeout)
        m_connection->setSendTimeout(*m_timeout);

    m_connection->connectAsync(
        endpointToVerify,
        std::bind(&AvailableEndpointVerificator::onConnectDone, this, _1));
}

SystemError::ErrorCode AvailableEndpointVerificator::lastSystemErrorCode() const
{
    return m_lastSystemErrorCode;
}

std::unique_ptr<AbstractStreamSocket> AvailableEndpointVerificator::takeSocket()
{
    decltype(m_connection) connection;
    connection.swap(m_connection);
    return connection;
}

void AvailableEndpointVerificator::stopWhileInAioThread()
{
    m_connection.reset();
}

void AvailableEndpointVerificator::onConnectDone(
    SystemError::ErrorCode sysErrorCode)
{
    m_lastSystemErrorCode = sysErrorCode;

    m_completionHandler(
        sysErrorCode == SystemError::noError
        ? VerificationResult::passed
        : VerificationResult::ioError);
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
