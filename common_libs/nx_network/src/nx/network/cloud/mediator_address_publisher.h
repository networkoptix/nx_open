#pragma once

#include <nx/network/cloud/mediator_server_connections.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/timer_manager.h>

namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API MediatorAddressPublisher:
    public aio::BasicPollable
{
    using BaseType = aio::BasicPollable;

public:
    static const std::chrono::milliseconds kDefaultRetryInterval;

    MediatorAddressPublisher(
        std::unique_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection);
    virtual ~MediatorAddressPublisher() override;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

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
    std::unique_ptr<hpm::api::MediatorServerTcpConnection> m_mediatorConnection;
    std::list<utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)>> m_updateHandlers;

    virtual void stopWhileInAioThread() override;

    void reportResultToTheCaller(hpm::api::ResultCode resultCode);
};

} // namespace cloud
} // namespace network
} // namespace nx
