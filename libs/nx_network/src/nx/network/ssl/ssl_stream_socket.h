// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <thread>

#include <nx/utils/thread/mutex.h>

#include "ssl_pipeline.h"
#include "../aio/stream_transforming_async_channel.h"
#include "../aio/timer.h"
#include "../socket_delegate.h"

namespace nx::network::ssl {

class Context;

namespace detail {

class NX_NETWORK_API StreamSocketToTwoWayPipelineAdapter:
    public utils::bstream::AbstractInput,
    public utils::bstream::AbstractOutput
{
public:
    StreamSocketToTwoWayPipelineAdapter(
        AbstractStreamSocket* streamSocket,
        aio::StreamTransformingAsyncChannel* asyncSslChannel);
    virtual ~StreamSocketToTwoWayPipelineAdapter() override;

    virtual int read(void* data, size_t count) override;
    virtual int write(const void* data, size_t count) override;

    void setFlagsForCallsInThread(std::thread::id threadId, int flags);

private:
    AbstractStreamSocket* m_streamSocket = nullptr;
    aio::StreamTransformingAsyncChannel* m_asyncSslChannel = nullptr;
    mutable nx::Mutex m_mutex;
    std::map<std::thread::id, int> m_threadIdToFlags;

    int bytesTransferredToPipelineReturnCode(int bytesTransferred);
    int getFlagsForCurrentThread() const;
};

} // namespace detail

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API StreamSocket:
    public CustomStreamSocketDelegate<AbstractEncryptedStreamSocket, AbstractStreamSocket>
{
    using base_type =
        CustomStreamSocketDelegate<AbstractEncryptedStreamSocket, AbstractStreamSocket>;

public:
    StreamSocket(
        Context* context,
        std::unique_ptr<AbstractStreamSocket> delegate,
        bool isServerSide,  // TODO: #akolesnikov Get rid of this argument.
        VerifyCertificateCallback verifyCertificateCallback);

    virtual ~StreamSocket() override;

    StreamSocket(const StreamSocket&) = delete;
    StreamSocket& operator=(const StreamSocket&) = delete;

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync() override;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        std::chrono::milliseconds timeout) override;

    virtual void connectAsync(
        const SocketAddress& address,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual int recv(void* buffer, std::size_t bufferLen, int flags = 0) override;

    virtual int send(const void* buffer, std::size_t bufferLen) override;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) override;

    virtual void sendAsync(
        const nx::Buffer* buffer,
        IoCompletionHandler handler) override;

    virtual bool isConnected() const override;

    virtual bool isEncryptionEnabled() const override;

    virtual void handshakeAsync(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual std::string serverName() const override;

    void setVerifyCertificateCallback(VerifyCertificateCallback func);
    void setServerName(const std::string& serverName);

protected:
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;

private:
    std::unique_ptr<aio::StreamTransformingAsyncChannel> m_asyncTransformingChannel;
    std::unique_ptr<AbstractStreamSocket> m_delegate;
    std::unique_ptr<ssl::Pipeline> m_sslPipeline;
    std::unique_ptr<detail::StreamSocketToTwoWayPipelineAdapter> m_socketToPipelineAdapter;
    utils::bstream::ProxyConverter m_proxyConverter;
    nx::Buffer m_emptyBuffer;
    aio::Timer m_handshakeTimer;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_handshakeHandler;
    std::optional<unsigned int> m_recvTimeoutBak;
    std::optional<unsigned int> m_sendTimeoutBak;
    std::optional<std::string> m_serverName;

    void startHandshakeTimer(std::chrono::milliseconds timout);
    void doHandshake();

    // TODO: #akolesnikov Make it virtual override after inheriting AbtractStreamSocket from aio::BasicPollable.
    void stopWhileInAioThread();
    void switchToSyncModeIfNeeded();
    void switchToAsyncModeIfNeeded();

    void handleSslError(int sslPipelineResultCode);

    bool saveTimeouts();
    bool restoreTimeouts();
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API ClientStreamSocket:
    public StreamSocket
{
    using base_type = StreamSocket;

public:
    ClientStreamSocket(
        Context* context,
        std::unique_ptr<AbstractStreamSocket> delegate,
        VerifyCertificateCallback verifyCertificateCallback);
};

class NX_NETWORK_API ServerSideStreamSocket:
    public StreamSocket
{
    using base_type = StreamSocket;

public:
    ServerSideStreamSocket(
        Context* context,
        std::unique_ptr<AbstractStreamSocket> delegate);
};

} // namespace nx::network::ssl
