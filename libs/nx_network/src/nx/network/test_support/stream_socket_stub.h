// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/aio/timer.h>
#include <nx/network/socket_delegate.h>
#include <nx/network/system_socket.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/std/optional.h>

namespace nx::network::test {

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
        const nx::Buffer* buffer,
        IoCompletionHandler handler) override;

    virtual SocketAddress getForeignAddress() const override;

    virtual bool setKeepAlive(std::optional<KeepAliveOptions> info) override;
    virtual bool getKeepAlive(std::optional<KeepAliveOptions>* result) const override;

    nx::Buffer read();
    void setConnectionToClosedState();
    void setForeignAddress(const SocketAddress& endpoint);

    void setPostDelay(std::optional<std::chrono::milliseconds> postDelay);
    void setSendCompletesAfterConnectionClosure(bool value);

protected:
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;

private:
    nx::Buffer* m_readBuffer = nullptr;
    IoCompletionHandler m_readHandler;
    nx::network::TCPSocket m_delegate;
    nx::utils::bstream::Pipe m_reflectingPipeline;
    SocketAddress m_foreignAddress;
    std::optional<KeepAliveOptions> m_keepAliveOptions;
    std::optional<std::chrono::milliseconds> m_postDelay;
    network::aio::Timer m_timer;
    bool m_connectionClosed = false;
    bool m_sendCompletesAfterConnectionClosure = true;

    void reportConnectionClosure();
};

} // namespace nx::network::test
