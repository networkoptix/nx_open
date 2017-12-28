#pragma once

#include <nx/network/aio/timer.h>
#include <nx/network/socket_delegate.h>
#include <nx/network/system_socket.h>
#include <nx/utils/byte_stream/pipeline.h>

namespace nx {
namespace network {
namespace test {

class NX_NETWORK_API StreamSocketStub:
    public nx::network::StreamSocketDelegate
{
    using base_type = nx::network::StreamSocketDelegate;

public:
    StreamSocketStub();
    ~StreamSocketStub();

    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual void readSomeAsync(
        nx::Buffer* const /*buffer*/,
        IoCompletionHandler handler) override;
    virtual void sendAsync(
        const nx::Buffer& buffer,
        IoCompletionHandler handler) override;
    virtual SocketAddress getForeignAddress() const override;

    virtual bool setKeepAlive(boost::optional<KeepAliveOptions> info) override;
    virtual bool getKeepAlive(boost::optional<KeepAliveOptions>* result) const override;

    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;

    QByteArray read();
    void setConnectionToClosedState();
    void setForeignAddress(const SocketAddress& endpoint);

    void setPostDelay(boost::optional<std::chrono::milliseconds> postDelay);

private:
    nx::Buffer* m_readBuffer = nullptr;
    IoCompletionHandler m_readHandler;
    nx::network::TCPSocket m_delegatee;
    nx::utils::bstream::Pipe m_reflectingPipeline;
    SocketAddress m_foreignAddress;
    boost::optional<KeepAliveOptions> m_keepAliveOptions;
    boost::optional<std::chrono::milliseconds> m_postDelay;
    network::aio::Timer m_timer;
};

} // namespace test
} // namespace network
} // namespace nx
