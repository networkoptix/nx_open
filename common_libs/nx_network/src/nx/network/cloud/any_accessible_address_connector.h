#pragma once

#include <chrono>
#include <deque>
#include <list>

#include <boost/optional.hpp>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_attributes_cache.h>
#include <nx/network/system_socket.h>
#include <nx/utils/move_only_func.h>

#include "cloud_address_connector.h"
#include "tunnel/tunnel_attributes.h"

namespace nx {
namespace network {
namespace cloud {

/**
 * Connects to any accessible address from the address list provided.
 */
class NX_NETWORK_API AnyAccessibleAddressConnector:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using ConnectHandler = nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode /*sysErrorCode*/,
        boost::optional<TunnelAttributes> /*cloudTunnelAttributes*/,
        std::unique_ptr<AbstractStreamSocket>)>;

    AnyAccessibleAddressConnector(
        int ipVersion,
        std::deque<AddressEntry> entries);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * @param timeout Zero means no timeout.
     */
    void connectAsync(
        std::chrono::milliseconds timeout,
        StreamSocketAttributes socketAttributes,
        ConnectHandler handler);

protected:
    virtual void stopWhileInAioThread() override;

    virtual std::unique_ptr<AbstractStreamSocket> createTcpSocket(int ipVersion);

private:
    const int m_ipVersion;
    std::deque<AddressEntry> m_entries;
    std::chrono::milliseconds m_timeout;
    StreamSocketAttributes m_socketAttributes;
    ConnectHandler m_handler;
    aio::Timer m_timer;
    int m_awaitedConnectOperationCount = 0;
    std::list<std::unique_ptr<AbstractStreamSocket>> m_directConnections;
    std::list<std::unique_ptr<CloudAddressConnector>> m_cloudConnectors;

    void connectToEntryAsync(const AddressEntry& dnsEntry);
    void onTimeout();

    bool establishDirectConnection(const SocketAddress& endpoint);
    void onDirectConnectDone(
        SystemError::ErrorCode sysErrorCode,
        std::list<std::unique_ptr<AbstractStreamSocket>>::iterator directConnectionIter);
    void onConnectDone(
        SystemError::ErrorCode sysErrorCode,
        boost::optional<TunnelAttributes> cloudTunnelAttributes,
        std::unique_ptr<AbstractStreamSocket> connection);
    void cleanUpAndReportResult(
        SystemError::ErrorCode sysErrorCode,
        boost::optional<TunnelAttributes> cloudTunnelAttributes,
        std::unique_ptr<AbstractStreamSocket> connection);

    void establishCloudConnection(const AddressEntry& dnsEntry);
    void onCloudConnectDone(
        SystemError::ErrorCode sysErrorCode,
        TunnelAttributes cloudTunnelAttributes,
        std::unique_ptr<AbstractStreamSocket> connection,
        std::list<std::unique_ptr<CloudAddressConnector>>::iterator connectorIter);
};

} // namespace cloud
} // namespace network
} // namespace nx
