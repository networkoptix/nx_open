#pragma once

#include <nx/network/socket_delegate.h>
#include <nx/network/system_socket.h>

namespace nx {
namespace cloud {
namespace relay {

class StreamSocketStub:
    public nx::network::StreamSocketDelegate
{
    using base_type = nx::network::StreamSocketDelegate;

public:
    StreamSocketStub();

    virtual void readSomeAsync(
        nx::Buffer* const /*buffer*/,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    void setConnectionToClosedState();

private:
    std::function<void(SystemError::ErrorCode, size_t)> m_readHandler;
    nx::network::TCPSocket m_delegatee;
};

} // namespace relay
} // namespace cloud
} // namespace nx
