// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "webrtc_ice.h"

namespace nx::webrtc {

class NX_WEBRTC_API IceUdp: public Ice
{
public:
    IceUdp(SessionPool* sessionPool, const SessionConfig& config);
    virtual ~IceUdp();
    std::optional<nx::network::SocketAddress> bind(const nx::network::HostAddress& host);
    void resetSocket(std::unique_ptr<nx::network::AbstractDatagramSocket>&& socket);
    void sendBinding(
        const IceCandidate& iceCandidate,
        const SessionDescription& sessionDescription);
    virtual IceCandidate::Filter type() const override;

protected:
    // From Ice.
    virtual nx::network::SocketAddress iceRemoteAddress() const override final;
    virtual nx::network::SocketAddress iceLocalAddress() const override final;
    virtual void stopUnsafe() override final;
    virtual void asyncSendPacketUnsafe() override final;

private:
    void onBytesRead(SystemError::ErrorCode errorCode, std::size_t bytesTransferred);

private:
    nx::Buffer m_readBuffer;
    std::unique_ptr<nx::network::AbstractDatagramSocket> m_socket;
    bool m_firstPacket = true;
    nx::network::aio::Timer m_keepAliveTimer;
    nx::network::aio::Timer m_bindingTimer;
    int m_bindingCount = 0;
};

} // namespace nx::webrtc
