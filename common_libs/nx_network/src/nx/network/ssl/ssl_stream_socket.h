#pragma once

#include <memory>

#include "ssl_pipeline.h"
#include "../aio/stream_transforming_async_channel.h"
#include "../socket_delegate.h"

namespace nx {
namespace network {
namespace ssl {

class NX_NETWORK_API StreamSocketToTwoWayPipelineAdapter:
    public utils::bstream::AbstractInput,
    public utils::bstream::AbstractOutput
{
public:
    StreamSocketToTwoWayPipelineAdapter(AbstractStreamSocket* streamSocket);
    virtual ~StreamSocketToTwoWayPipelineAdapter() override;

    virtual int read(void* data, size_t count) override;
    virtual int write(const void* data, size_t count) override;

private:
    AbstractStreamSocket* m_streamSocket;

    int bytesTransferredToPipelineReturnCode(int bytesTransferred);
};

//-------------------------------------------------------------------------------------------------

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

    virtual int recv(void* buffer, unsigned int bufferLen, int flags = 0) override;

    virtual int send(const void* buffer, unsigned int bufferLen) override;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) override;

    virtual void sendAsync(
        const nx::Buffer& buffer,
        IoCompletionHandler handler) override;

    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler) override;

    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;

private:
    std::unique_ptr<aio::StreamTransformingAsyncChannel> m_asyncTransformingChannel;
    std::unique_ptr<AbstractStreamSocket> m_delegatee;
    std::unique_ptr<ssl::Pipeline> m_sslPipeline;
    StreamSocketToTwoWayPipelineAdapter m_socketToPipelineAdapter;
    utils::bstream::ProxyConverter m_proxyConverter;

    // TODO: #ak Make it virtual override after inheriting AbtractStreamSocket from aio::BasicPollable.
    void stopWhileInAioThread();
    void switchToSyncModeIfNeeded();
    void switchToAsyncModeIfNeeded();
};

} // namespace ssl
} // namespace network
} // namespace nx
