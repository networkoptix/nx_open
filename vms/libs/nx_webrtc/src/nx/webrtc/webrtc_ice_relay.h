// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/system_socket.h>

#include "webrtc_ice.h"

namespace nx::webrtc {

class NX_WEBRTC_API IceRelay: public Ice
{
public:
    IceRelay(
        SessionPool* sessionPool,
        const SessionConfig& config,
        const std::string& sessionId);
    virtual ~IceRelay();
    void setRemoteSrflx(const IceCandidate& candidate);
    void setOauthInfo(const OauthInfo& oauth);
    void startRelay(const TurnServerInfo& turnServer);
    virtual IceCandidate::Filter type() const override;

protected:
    // From Ice.
    virtual nx::network::SocketAddress iceRemoteAddress() const override final;
    virtual nx::network::SocketAddress iceLocalAddress() const override final;
    virtual void stopUnsafe() override final;
    virtual void asyncSendPacketUnsafe() override final;
    virtual nx::Buffer toSendBuffer(const char* data, int dataSize) const override final;

private:
    void writeRelayPacket(const char* data, int size);
    void onBytesRead(SystemError::ErrorCode errorCode, std::size_t bytesTransferred);
    bool processRelayPackets();
    void bindToRelay();
    void sendAllocate();
    void sendRefresh();
    void scheduleRefresh();
    void createPermission();
    void fillAuthorizationAndIntegrity(stun::Message& message);
    bool handleBinding(const stun::Message& message);
    bool handleAllocateNoAuth(const stun::Message& message);
    bool handleAllocate(const stun::Message& message);
    bool handleCreatePermission(const stun::Message& message);
    bool handleIndication(const stun::Message& message);
    bool handleLongTermAuthorization(const stun::Message& message);
    bool handleThirdPartyAuthorization(const stun::Message& message);
    bool handleRefresh(const stun::Message& message);

private:
    enum RelayStage
    {
        Binding,
        AllocateNoAuth,
        Allocate,
        CreatePermission,
        Indication,
        Unknown
    };
    enum AuthorizationType
    {
        None,
        LongTerm,
        Oauth
    };

private:
    nx::Buffer m_readBuffer;

    nx::network::TCPSocket m_socket;
    TurnServerInfo m_turnServer;
    std::optional<OauthInfo> m_oauthInfo;
    std::string m_nonce;
    std::string m_realm;
    std::string m_password;
    AuthorizationType m_turnAuth = AuthorizationType::None;
    const std::string m_sessionId;
    RelayStage m_relayStage = RelayStage::Binding;
    std::optional<IceCandidate> m_remoteSrflx;
    std::optional<nx::network::SocketAddress> m_peerAddress;
    std::optional<IceCandidate> m_localRelay;
    nx::network::aio::Timer m_keepAliveTimer;
    nx::network::aio::Timer m_createPermissionTimer;
    nx::network::aio::Timer m_refreshTimer;
    int m_refreshLifetimeSeconds = 600;
};

} // namespace nx::webrtc
