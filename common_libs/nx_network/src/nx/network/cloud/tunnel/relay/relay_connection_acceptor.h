#pragma once

#include <memory>

#include <nx/network/abstract_acceptor.h>
#include <nx/network/socket_common.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {

class NX_NETWORK_API ConnectionAcceptor:
    public AbstractAcceptor
{
    using base_type = AbstractAcceptor;

public:
    ConnectionAcceptor(const SocketAddress& relayEndpoint);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOSync() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const SocketAddress m_relayEndpoint;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API ConnectionAcceptorFactory:
    public nx::utils::BasicFactory<
        std::unique_ptr<AbstractAcceptor>(const SocketAddress& /*relayEndpoint*/)>
{
    using base_type = nx::utils::BasicFactory<
        std::unique_ptr<AbstractAcceptor>(const SocketAddress& /*relayEndpoint*/)>;

public:
    ConnectionAcceptorFactory();

    static ConnectionAcceptorFactory& instance();

private:
    std::unique_ptr<AbstractAcceptor> defaultFactoryFunc(
        const SocketAddress& relayEndpoint);
};

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
