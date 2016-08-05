#pragma once

#include <nx/utils/timer_manager.h>
#include <nx/network/cloud/mediator_connections.h>

namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API MediatorAddressPublisher:
    public QnStoppableAsync
{
public:
    static const std::chrono::milliseconds kDefaultRetryInterval;

    MediatorAddressPublisher(
        std::shared_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection);

    void bindToAioThread(AbstractAioThread* aioThread);
    void setRetryInterval(std::chrono::milliseconds interval);
    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    void updateAddresses(
        std::list<SocketAddress> addresses,
        utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)> updateHandler = nullptr);

private:
    void publishAddressesIfNeeded();

private:
    std::chrono::milliseconds m_retryInterval;
    bool m_isRequestInProgress;
    std::list<SocketAddress> m_serverAddresses;
    std::list<SocketAddress> m_publishedAddresses;
    std::shared_ptr<hpm::api::MediatorServerTcpConnection> m_mediatorConnection;
    utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)> m_updateHandler;
};

} // namespace cloud
} // namespace network
} // namespace nx
