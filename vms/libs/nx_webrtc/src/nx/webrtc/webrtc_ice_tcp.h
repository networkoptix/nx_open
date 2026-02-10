// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/buffered_stream_socket.h>
#include <nx/utils/scope_guard.h>

#include "network/tcp_connection_processor.h"
#include "network/tcp_listener.h"
#include "webrtc_ice.h"

class SessionPool;

namespace nx::webrtc {

class NX_WEBRTC_API ListenerTcp:
    public QnTCPConnectionProcessor
{
public:
    ListenerTcp(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner,
        SessionPool* sessionPool,
        int64_t maxTcpRequestSize);

protected:
    // From TCPConnectionProcessor.
    virtual void run() override final;

private:
    SessionPool* m_sessionPool;
};

class IceTcp: public Ice
{
public:
    IceTcp(
        std::unique_ptr<nx::network::AbstractStreamSocket>&& socket,
        SessionPool* sessionPool,
        const SessionConfig& config,
        const QByteArray& readBuffer
        );
    virtual ~IceTcp();
    void start();
    virtual IceCandidate::Filter type() const override;
    bool isStopped();

protected:
    // From Ice.
    virtual nx::network::SocketAddress iceRemoteAddress() const override final;
    virtual nx::network::SocketAddress iceLocalAddress() const override final;
    virtual void stopUnsafe() override final;
    virtual void asyncSendPacketUnsafe() override final;
    virtual nx::Buffer toSendBuffer(const char* data, int dataSize) const override final;

private:
    void onBytesRead(SystemError::ErrorCode errorCode, std::size_t bytesTransferred);

private:
    nx::network::BufferedStreamSocket m_socket;
    nx::Buffer m_readBuffer;
    std::atomic<bool> m_stopped = false;
};

} // namespace nx::webrtc
