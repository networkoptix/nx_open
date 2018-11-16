#pragma once

#include <nx/network/aio/abstract_async_channel.h>
#include <nx/network/p2p_socket/p2p_transport_mode.h>
#include <memory>

namespace nx::network::detail {

class IP2PSocketDelegate;

class P2PSocketBase: public aio::AbstractAsyncChannel
{
public:
    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

private:
    std::unique_ptr<detail::IP2PSocketDelegate> m_socketDelegate;
};

} // namespace nx::network::detail