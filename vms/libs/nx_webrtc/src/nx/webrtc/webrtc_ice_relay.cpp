// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_ice_relay.h"

#include <nx/utils/cryptographic_hash.h>

#include "webrtc_session_pool.h"

namespace nx::webrtc {

constexpr size_t kRawReadBufferSize = 65536;
constexpr size_t kOauthKeySize = 16;
constexpr std::chrono::seconds kKeepAliveTimeout{15};

// Can't detect permissions lifetime in runtime,
// but default lifetime in coturn is 300s.
constexpr std::chrono::seconds kPermissionsLifetime{295};

IceRelay::IceRelay(
    SessionPool* sessionPool,
    const SessionConfig& config,
    const std::string& sessionId
    )
    :
    Ice(sessionPool, config),
    m_sessionId(sessionId)
{
    m_readBuffer.reserve(kRawReadBufferSize);

    m_socket.bindToAioThread(m_pollable.getAioThread());
    m_keepAliveTimer.bindToAioThread(m_pollable.getAioThread());
    m_createPermissionTimer.bindToAioThread(m_pollable.getAioThread());
    m_refreshTimer.bindToAioThread(m_pollable.getAioThread());

    m_socket.setSendBufferSize(kIceTcpSendBufferSize);
    m_socket.setNonBlockingMode(true);
}

/*virtual*/ IceRelay::~IceRelay()
{
    std::promise<void> stoppedPromise;
    m_pollable.dispatch(
        [this, &stoppedPromise]()
        {
            stopUnsafe();
            stoppedPromise.set_value();
        });
    stoppedPromise.get_future().wait();
}

/*virtual*/ void IceRelay::stopUnsafe()
{
    if (!m_needStop)
    {
        m_keepAliveTimer.cancelSync();
        m_createPermissionTimer.cancelSync();
        m_refreshTimer.cancelSync();
        m_socket.cancelIOSync(aio::EventType::etAll);

        Ice::stopUnsafe(); //< Dangerous, object can be destroyed after this call.
    }
}

/*virtual*/ IceCandidate::Filter IceRelay::type() const
{
    return IceCandidate::Filter::UdpRelay;
}

void IceRelay::startRelay(const TurnServerInfo& turnServer)
{
    m_turnServer = turnServer;

    m_socket.connectAsync(
        m_turnServer.address,
        [this](SystemError::ErrorCode errorCode)
        {
            if (errorCode != SystemError::noError)
                stopUnsafe();
            else
                bindToRelay();
        });
}

void IceRelay::setRemoteSrflx(const IceCandidate& candidate)
{
    // Guarded by IceManager, so can't be called in parallel with ~IceRelay().
    m_pollable.dispatch(
        [this, candidate]()
        {
            if (m_needStop)
                return;
            m_remoteSrflx = candidate;
            if (m_relayStage == RelayStage::CreatePermission)
            {
                NX_DEBUG(this, "TURN: create permission for %1", m_sessionId);
                createPermission();
            }
        });
}

void IceRelay::setOauthInfo(const OauthInfo& oauth)
{
    m_pollable.dispatch(
        [this, oauth]()
        {
            m_oauthInfo = oauth;
            NX_ASSERT(m_relayStage == RelayStage::Allocate);
            sendAllocate();
        });
}

void IceRelay::bindToRelay()
{
    stun::Message request(stun::Header(stun::MessageClass::request, stun::MethodType::bindingMethod));

    stun::MessageSerializer stunSerializer;
    stunSerializer.setAlwaysAddFingerprint(false);
    nx::Buffer sendBuffer = stunSerializer.serialized(request);
    writeRelayPacket(sendBuffer.data(), sendBuffer.size());

    NX_DEBUG(this, "TURN: binding sent for session %1", m_sessionId);

    m_socket.readSomeAsync(
        &m_readBuffer,
        [this](auto... args)
        {
            onBytesRead(std::move(args)...);
        });
}

/*virtual*/ nx::Buffer IceRelay::toSendBuffer(const char* data, int dataSize) const
{
    if (dataSize > std::numeric_limits<uint16_t>::max())
    {
        NX_WARNING(this, "Can't send message: size is too large %1 for %2", dataSize, m_sessionId);
        return nx::Buffer();
    }

    NX_CRITICAL(m_remoteSrflx);
    stun::Message response(stun::Header(stun::MessageClass::indication, stun::MethodType::sendMethod));
    // m_peerAddress and m_remoteSrflx::address should match, but sometimes they don't.
    // Probably the reason is that external port of sender is unexpectedly changed.
    if (m_peerAddress)
        response.newAttribute<stun::attrs::XorPeerAddress>(*m_peerAddress);
    else
        response.newAttribute<stun::attrs::XorPeerAddress>(m_remoteSrflx->address);
    response.newAttribute<stun::attrs::Data>(nx::Buffer(data, dataSize));

    stun::MessageSerializer stunSerializer;
    stunSerializer.setAlwaysAddFingerprint(false);
    return stunSerializer.serialized(response);
}

void IceRelay::fillAuthorizationAndIntegrity(stun::Message& message)
{
    switch (m_turnAuth)
    {
        case AuthorizationType::None:
            return;
        case AuthorizationType::LongTerm:
            message.newAttribute<stun::attrs::UserName>(m_turnServer.user);
            message.newAttribute<stun::attrs::Realm>(m_realm);
            message.newAttribute<stun::attrs::Nonce>(m_nonce);
            message.insertIntegrity(m_turnServer.user, m_password);
            return;
        case AuthorizationType::Oauth:
            NX_CRITICAL(m_oauthInfo);
            message.newAttribute<stun::attrs::Realm>(m_realm);
            message.newAttribute<stun::attrs::Nonce>(m_nonce);
//            message.newAttribute<stun::attrs::Lifetime>(m_oauthInfo->lifetime);
            message.newAttribute<stun::attrs::AccessToken>(m_oauthInfo->accessToken);
            // Use first 16 bytes of key, see https://github.com/coturn/coturn/issues/189.
            message.insertIntegrity(
                m_oauthInfo->kid,
                m_oauthInfo->sessionKey.substr(0, kOauthKeySize));
            return;
    }
}

void IceRelay::createPermission()
{
    NX_CRITICAL(m_remoteSrflx);

    stun::MessageSerializer stunSerializer;
    stunSerializer.setAlwaysAddFingerprint(false);
    stun::Message request(
        stun::Header(stun::MessageClass::request, stun::MethodType::createPermissionMethod));
    request.newAttribute<stun::attrs::XorPeerAddress>(m_remoteSrflx->address);
    fillAuthorizationAndIntegrity(request);

    nx::Buffer sendBuffer = stunSerializer.serialized(request);
    writeRelayPacket(sendBuffer.data(), sendBuffer.size());
}

void IceRelay::sendRefresh()
{
    stun::MessageSerializer stunSerializer;
    stunSerializer.setAlwaysAddFingerprint(false);
    stun::Message request(
        stun::Header(stun::MessageClass::request, stun::MethodType::refreshMethod));
    request.newAttribute<stun::attrs::Lifetime>(m_refreshLifetimeSeconds);
    fillAuthorizationAndIntegrity(request);

    nx::Buffer sendBuffer = stunSerializer.serialized(request);
    writeRelayPacket(sendBuffer.data(), sendBuffer.size());
}

void IceRelay::scheduleRefresh()
{
    auto refreshLifetime = std::chrono::seconds(std::max(m_refreshLifetimeSeconds - 5, 5));
    NX_VERBOSE(this, "TURN schedule refresh (lifetime=%1) for %2",
        refreshLifetime, m_sessionId);

    m_refreshTimer.start(
        refreshLifetime,
        [this]()
        {
            NX_VERBOSE(
                this,
                "Renew allocation for %1",
                m_sessionId);
            sendRefresh();
        });
}

bool IceRelay::handleRefresh(const stun::Message& message)
{
    if (message.header.method != stun::MethodType::refreshMethod
        || message.header.messageClass != stun::MessageClass::successResponse)
    {
        NX_WARNING(this, "TURN unexpected message for Refresh stage: %1/%2 for %3",
            message.header.messageClass, message.header.method, m_sessionId);
        return false;
    }
    stun::MessageSerializer stunSerializer;
    stunSerializer.setAlwaysAddFingerprint(false);

    auto lifetime = message.getAttribute<stun::attrs::Lifetime>();
    if (lifetime)
        m_refreshLifetimeSeconds = lifetime->value();

    scheduleRefresh();

    return true;
}

bool IceRelay::handleBinding(const stun::Message& message)
{
    if (message.header.messageClass != stun::MessageClass::successResponse
        || message.header.method != stun::MethodType::bindingMethod)
    {
        NX_WARNING(this, "TURN unexpected message for Binding stage: %1/%2 for %3",
            message.header.messageClass, message.header.method, m_sessionId);
        return false;
    }
    stun::MessageSerializer stunSerializer;
    stunSerializer.setAlwaysAddFingerprint(false);

    stun::Message request(stun::Header(stun::MessageClass::request, stun::MethodType::allocateMethod));
    request.newAttribute<stun::attrs::RequestedTransport>();

    nx::Buffer sendBuffer = stunSerializer.serialized(request);
    writeRelayPacket(sendBuffer.data(), sendBuffer.size());

    m_relayStage = RelayStage::AllocateNoAuth;
    return true;
}

bool IceRelay::handleLongTermAuthorization(const stun::Message& message)
{
    m_turnAuth = AuthorizationType::LongTerm;
    auto nonce = message.getAttribute<stun::attrs::Nonce>();
    if (!nonce)
        m_nonce = nx::Uuid::createUuid().toStdString(QUuid::WithBraces);
    else
        m_nonce = nonce->getString();

    auto realm = message.getAttribute<stun::attrs::Realm>();
    if (!realm)
        m_realm = m_turnServer.realm;
    else
        m_realm = realm->getString();
    if (m_turnServer.longTermAuth)
    {
        m_password = nx::utils::md5(
            QByteArray::fromStdString(
                m_turnServer.user +
                ":" +
                m_realm +
                ":" +
                m_turnServer.password))
            .toStdString();
    }
    else
    {
        m_password = m_turnServer.password;
    }
    return true;
}

bool IceRelay::handleThirdPartyAuthorization(const stun::Message& message)
{
    auto nonce = message.getAttribute<stun::attrs::Nonce>();
    if (!nonce)
        m_nonce = nx::Uuid::createUuid().toStdString(QUuid::WithBraces);
    else
        m_nonce = nonce->getString();

    auto realm = message.getAttribute<stun::attrs::Realm>();
    if (!realm)
        m_realm = m_turnServer.realm;
    else
        m_realm = realm->getString();

    m_turnAuth = AuthorizationType::Oauth;
    if (!m_oauthInfo)
    {
        auto thirdPartyAuthorization = message.getAttribute<stun::attrs::ThirdPartyAuthorization>();
        NX_CRITICAL(thirdPartyAuthorization);

        m_sessionPool->iceManager().getThirdPartyAuthorization(
            m_sessionId,
            thirdPartyAuthorization->getString());
        return false;
    }
    return true;
}

void IceRelay::sendAllocate()
{
    stun::MessageSerializer stunSerializer;
    stunSerializer.setAlwaysAddFingerprint(false);

    stun::Message request(stun::Header(stun::MessageClass::request, stun::MethodType::allocateMethod));
    request.newAttribute<stun::attrs::RequestedTransport>();
    fillAuthorizationAndIntegrity(request);

    nx::Buffer sendBuffer = stunSerializer.serialized(request);
    writeRelayPacket(sendBuffer.data(), sendBuffer.size());
}

bool IceRelay::handleAllocateNoAuth(const stun::Message& message)
{
    if (message.header.method != stun::MethodType::allocateMethod)
    {
        NX_WARNING(this, "TURN unexpected message for AllocateNoAuth stage: %1/%2 for %3",
            message.header.messageClass, message.header.method, m_sessionId);
        return false;
    }

    m_relayStage = RelayStage::Allocate;

    if (message.header.messageClass == stun::MessageClass::errorResponse)
    {
        auto thirdPartyAuthorization = message.getAttribute<stun::attrs::ThirdPartyAuthorization>();

        // Once fill authorization info.
        if ((thirdPartyAuthorization && !handleThirdPartyAuthorization(message)) ||
            (!thirdPartyAuthorization && !handleLongTermAuthorization(message)))
            return true;

        sendAllocate();
        return true;
    }
    else
    {
        return handleAllocate(message);
    }
}

bool IceRelay::handleAllocate(const stun::Message& message)
{
    if (message.header.method != stun::MethodType::allocateMethod
        || message.header.messageClass != stun::MessageClass::successResponse)
    {
        NX_WARNING(this, "TURN unexpected message for Allocate stage: %1/%2 for %3",
            message.header.messageClass, message.header.method, m_sessionId);
        return false;
    }
    stun::MessageSerializer stunSerializer;
    stunSerializer.setAlwaysAddFingerprint(false);

    auto xorMappedAddress = message.getAttribute<stun::attrs::XorMappedAddress>();
    auto xorRelayedAddress = message.getAttribute<stun::attrs::XorRelayedAddress>();
    auto lifetime = message.getAttribute<stun::attrs::Lifetime>();
    if (lifetime)
        m_refreshLifetimeSeconds = lifetime->value();

    if (!xorMappedAddress || !xorRelayedAddress)
    {
        NX_DEBUG(this, "TURN no mapped or relayed address in Allocate response %1", m_sessionId);
        return false;
    }

    m_localRelay = IceCandidate{
        .protocol = IceCandidate::Protocol::UDP,
        .address = xorRelayedAddress->addr(),
        .type = IceCandidate::Type::Relay,
        .relatedAddress = xorMappedAddress->addr()
    };

    m_relayStage = RelayStage::CreatePermission;
    if (!m_remoteSrflx)
        NX_VERBOSE(
            this,
            "TURN waiting from Srflx for remote peer to make CreatePermission request %1",
            m_sessionId);
    else
        createPermission();

    scheduleRefresh();

    return true;
}

bool IceRelay::handleCreatePermission(const stun::Message& message)
{
    if (!m_remoteSrflx)
    {
        NX_WARNING(this, "TURN unexpected message while waiting for remote srflx: %1/%2 for %3",
            message.header.messageClass, message.header.method, m_sessionId);
        return false;
    }

    if (message.header.method != stun::MethodType::createPermissionMethod
        || message.header.messageClass != stun::MessageClass::successResponse)
    {
        NX_WARNING(this, "TURN unexpected message for CreatePermission stage: %1/%2 for %3",
            message.header.messageClass, message.header.method, m_sessionId);
        return false;
    }
    if (m_relayStage != RelayStage::Indication)
    {
        m_sessionPool->gotIceFromManager(
            m_sessionId,
            *m_localRelay);
        m_relayStage = RelayStage::Indication; //< Ready to send and receive packets to/from remote peer.
    }

    NX_VERBOSE(this, "TURN schedule renew permissions (lifetime=%1) for %2",
        kPermissionsLifetime, m_sessionId);

    m_createPermissionTimer.start(
        kPermissionsLifetime,
        [this]()
        {
            NX_VERBOSE(
                this,
                "Renew permissions for %1",
                m_sessionId);
            createPermission();
        });

    return true;
}

bool IceRelay::handleIndication(const stun::Message& message)
{
    if (message.header.method == stun::MethodType::createPermissionMethod)
    {
        return handleCreatePermission(message);
    }

    if (message.header.method == stun::MethodType::refreshMethod)
    {
        return handleRefresh(message);
    }

    if (message.header.messageClass != stun::MessageClass::indication
        || message.header.method != stun::MethodType::dataMethod)
    {
        NX_DEBUG(this, "TURN unexpected message in Indication stage: %1/%2 for %3",
            message.header.messageClass, message.header.method, m_sessionId);
        return true; //< Not a fatal error.
    }
    if (auto peerAddress = message.getAttribute<stun::attrs::XorPeerAddress>())
        m_peerAddress = peerAddress->addr();

    auto data = message.getAttribute<stun::attrs::Data>();
    if (!data)
    {
        NX_DEBUG(this, "TURN no data field in Indication for %1", m_sessionId);
        return true; //< Not a fatal error.
    }
    const auto& buffer = data->getBuffer();
    return processIncomingPacket((const uint8_t*) buffer.data(), buffer.size());
}

bool IceRelay::processRelayPackets()
{
    while (!m_readBuffer.empty())
    {
        stun::Message message;
        stun::MessageParser stunParser;
        stunParser.setMessage(&message);
        size_t parsed = 0;

        if (stunParser.parse(m_readBuffer, &parsed) != nx::network::server::ParserState::done)
        {
            NX_DEBUG(this, "TURN message not parsed for %1", m_sessionId);
            return true; //< Not a fatal error.
        }

        switch (m_relayStage)
        {
            case RelayStage::Binding:
            {
                if (!handleBinding(message))
                    return false;
                break;
            }
            case RelayStage::AllocateNoAuth:
            {
                if (!handleAllocateNoAuth(message))
                    return false;
                break;
            }
            case RelayStage::Allocate:
            {
                if (!handleAllocate(message))
                    return false;
                break;
            }
            case RelayStage::CreatePermission:
            {
                if (!handleCreatePermission(message))
                    return false;
                break;
            }
            case RelayStage::Indication:
            {
                if (!handleIndication(message))
                    return false;
                break;
            }
            default:
            {
                NX_ASSERT(false, "Unexpected state: %1 for %2", static_cast<int>(m_relayStage), m_sessionId);
                return false;
            }
        }

        m_readBuffer.erase(0, parsed);
    }
    return true;
}

void IceRelay::writeRelayPacket(const char* data, int size)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    // Always foreground.
    NX_ASSERT(m_foregroundOffset <= m_sendBuffers.size());
    m_sendBuffers.insert(m_sendBuffers.begin() + m_foregroundOffset, {data, (size_t) size});
    ++m_foregroundOffset;

    sendNextBufferUnsafe();
}

void IceRelay::onBytesRead(SystemError::ErrorCode errorCode, std::size_t bytesTransferred)
{
    if (m_needStop)
        return;

    if (nx::network::socketCannotRecoverFromError(errorCode) || bytesTransferred == 0)
    {
        stopUnsafe();
    }
    else
    {
        bool result = processRelayPackets();
        if (result)
        {
            m_keepAliveTimer.start(
                kKeepAliveTimeout,
                [this]()
                {
                    NX_DEBUG(
                        this,
                        "Keep alive timeout reached, stopping Relay ice for %1",
                        m_sessionId);
                    stopUnsafe(); //< Ice restart is better.
                });
            m_socket.readSomeAsync(
                &m_readBuffer,
                [this](auto... args)
                {
                    onBytesRead(std::move(args)...);
                });
        }
        else
        {
            stopUnsafe();
        }
    }
}

/*virtual*/ void IceRelay::asyncSendPacketUnsafe()
{
    m_socket.post(
        [this]()
        {
            if (m_needStop)
                return;
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_socket.sendAsync(
                &m_sendBuffers.front(),
                [this](auto... args)
                {
                    onBytesWritten(std::move(args)...);
                });
        });
}

/*virtual*/ nx::network::SocketAddress IceRelay::iceRemoteAddress() const
{
    return m_socket.getForeignAddress();
}

/*virtual*/ nx::network::SocketAddress IceRelay::iceLocalAddress() const
{
    return m_socket.getLocalAddress();
}

} // namespace nx::webrtc
