#pragma once

#include <nx/network/socket_delegate.h>
#include <nx/network/system_socket.h>
#include <nx/utils/byte_stream/pipeline.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class StreamSocketStub:
    public nx::network::StreamSocketDelegate
{
    using base_type = nx::network::StreamSocketDelegate;

public:
    StreamSocketStub();

    virtual void readSomeAsync(
        nx::Buffer* const /*buffer*/,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    virtual void sendAsync(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    virtual SocketAddress getForeignAddress() const override;

    QByteArray read();
    void setConnectionToClosedState();
    void setForeignAddress(const SocketAddress& endpoint);

private:
    std::function<void(SystemError::ErrorCode, size_t)> m_readHandler;
    nx::network::TCPSocket m_delegatee;
    nx::utils::bstream::ReflectingPipeline m_reflectingPipeline;
    SocketAddress m_foreignAddress;
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
