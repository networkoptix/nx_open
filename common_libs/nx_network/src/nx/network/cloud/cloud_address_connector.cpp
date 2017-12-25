#include "cloud_address_connector.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/socket_global.h>

namespace nx {
namespace network {
namespace cloud {

CloudAddressConnector::CloudAddressConnector(
    const AddressEntry& targetHostAddress,
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes)
    :
    m_targetHostAddress(targetHostAddress),
    m_timeout(timeout),
    m_socketAttributes(socketAttributes)
{
}

CloudAddressConnector::~CloudAddressConnector()
{
    m_asyncConnectGuard->terminate();
}

void CloudAddressConnector::connectAsync(ConnectHandler handler)
{
    m_handler = std::move(handler);
    auto sharedOperationGuard = m_asyncConnectGuard.sharedGuard();

    SocketGlobals::cloud().outgoingTunnelPool().establishNewConnection(
        m_targetHostAddress,
        m_timeout,
        m_socketAttributes,
        [this, sharedOperationGuard](
            SystemError::ErrorCode errorCode,
            TunnelAttributes tunnelAttributes,
            std::unique_ptr<AbstractStreamSocket> cloudConnection)
        {
            // NOTE: this handler is called from unspecified thread.
            auto operationLock = sharedOperationGuard->lock();
            if (!operationLock)
                return; //< Operation has been cancelled.

            dispatch(
                [this, errorCode, tunnelAttributes = std::move(tunnelAttributes),
                    cloudConnection = std::move(cloudConnection)]() mutable
                {
                    nx::utils::swapAndCall(
                        m_handler,
                        errorCode,
                        std::move(tunnelAttributes),
                        std::move(cloudConnection));
                });
        });
}

void CloudAddressConnector::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();
    m_asyncConnectGuard->terminate();
}

} // namespace cloud
} // namespace network
} // namespace nx
