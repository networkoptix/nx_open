#pragma once

#include <memory>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {

class NX_NETWORK_API ConnectionAcceptor:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    ConnectionAcceptor(const SocketAddress& relayEndpoint);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void getNextSocketAsync(
        nx::utils::MoveOnlyFunc<void(std::unique_ptr<AbstractStreamSocket>)> handler);
    void cancelAccept();

protected:
    virtual void stopWhileInAioThread() override;

private:
    const SocketAddress m_relayEndpoint;
};

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
