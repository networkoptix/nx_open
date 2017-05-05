#pragma once

#include <memory>

#include <nx/network/abstract_acceptor.h>
#include <nx/network/socket_common.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

#include "../../cloud_abstract_connection_acceptor.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {

class NX_NETWORK_API ConnectionAcceptor:
    public AbstractConnectionAcceptor
{
    using base_type = AbstractConnectionAcceptor;

public:
    ConnectionAcceptor(const SocketAddress& relayEndpoint);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOSync() override;

    virtual std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const SocketAddress m_relayEndpoint;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API ConnectionAcceptorFactory:
    public nx::utils::BasicFactory<
        std::unique_ptr<AbstractConnectionAcceptor>(const SocketAddress& /*relayEndpoint*/)>
{
    using base_type = nx::utils::BasicFactory<
        std::unique_ptr<AbstractConnectionAcceptor>(const SocketAddress& /*relayEndpoint*/)>;

public:
    ConnectionAcceptorFactory();

    static ConnectionAcceptorFactory& instance();

private:
    std::unique_ptr<AbstractConnectionAcceptor> defaultFactoryFunc(
        const SocketAddress& relayEndpoint);
};

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
