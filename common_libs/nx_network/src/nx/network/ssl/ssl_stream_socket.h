#pragma once

#include <memory>

#include "ssl_pipeline.h"
#include "../aio/stream_transforming_async_channel.h"
#include "../socket_delegate.h"

namespace nx {
namespace network {
namespace ssl {

class NX_NETWORK_API StreamSocket:
    public StreamSocketDelegate
{
    using base_type = StreamSocketDelegate;

public:
    StreamSocket(
        std::unique_ptr<AbstractStreamSocket> delegatee,
        bool isServerSide);  // TODO: #ak Get rid of this one.

    virtual ~StreamSocket() override;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    virtual void sendAsync(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler) override;

    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;

private:
    std::unique_ptr<aio::StreamTransformingAsyncChannel> m_asyncTransformingChannel;
    std::unique_ptr<AbstractStreamSocket> m_delegatee;

    void stopWhileInAioThread();
};

} // namespace ssl
} // namespace network
} // namespace nx
