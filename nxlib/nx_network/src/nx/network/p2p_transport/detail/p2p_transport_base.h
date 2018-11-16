#pragma once

#include "i_p2p_transport_delegate.h"
#include <memory>

namespace nx::network::detail {

class P2PTransportBase: public IP2PSocketDelegate
{
public:
    P2PTransportBase(std::unique_ptr<detail::IP2PSocketDelegate> transportDelegate);

    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler) override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void pleaseStopSync() override;
    virtual SocketAddress getForeignAddress() const override;

private:
    std::unique_ptr<detail::IP2PSocketDelegate> m_transportDelegate;
};

} // namespace nx::network::detail