#pragma once

#include <memory>

#include "abstract_async_connector.h"

namespace nx::network::aio {

class NX_NETWORK_API StreamSocketConnector:
    public AbstractAsyncConnector
{
    using base_type = AbstractAsyncConnector;

public:
    virtual void bindToAioThread(AbstractAioThread* aioThread) override;

    virtual void connectAsync(
        const network::SocketAddress& targetEndpoint,
        ConnectHandler handler) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<AbstractStreamSocket> m_streamSocket;
};

} // namespace nx::network::aio
