// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_srflx.h"

#include "webrtc_session_pool.h"

namespace nx::webrtc {

constexpr size_t kRawReadBufferSize = nx::network::AbstractDatagramSocket::MAX_DATAGRAM_SIZE;
constexpr std::chrono::seconds kPunchTimeout{2};

using namespace nx::network;

Srflx::Srflx(const std::string& sessionId,
    const std::vector<nx::network::SocketAddress>& stunServers,
    const Handler& handler)
    :
    m_sessionId(sessionId),
    m_socket(nx::network::SocketFactory::createDatagramSocket()),
    m_handler(handler)
{
    m_pollable.bindToAioThread(m_socket->getAioThread());
    for (const auto& stunServer: stunServers)
        m_addrQueue.push_back(stunServer);

    m_socket->bind(nx::network::SocketAddress(nx::network::HostAddress::anyHost, 0));
    m_socket->setRecvTimeout(kPunchTimeout);
    m_socket->setSendTimeout(kPunchTimeout);

    // Same request for all servers.
    stun::Message request(stun::Header(stun::MessageClass::request, stun::MethodType::bindingMethod));
    stun::MessageSerializer stunSerializer;
    stunSerializer.setAlwaysAddFingerprint(true);
    m_sendBuffer = stunSerializer.serialized(request);

    // Do sync Punch
    for (const auto& addr: m_addrQueue)
    {
        NX_DEBUG(
            this,
            "%1: Punching srflx: binded to %2, STUN server: %3",
            m_sessionId,
            m_socket->getLocalAddress().toString(),
            addr.toString());

        m_socket->sendTo(m_sendBuffer.data(), m_sendBuffer.size(), addr);
    }

    m_readBuffer.reserve(kRawReadBufferSize);
    m_timeout.restart();
    m_socket->setNonBlockingMode(true);
    m_socket->readSomeAsync(
        &m_readBuffer,
        [this](auto... args)
        {
            onBytesRead(std::move(args)...);
        });
}

Srflx::~Srflx()
{
    m_pollable.executeInAioThreadSync([this]()
        {
            if (m_socket)
                m_socket->pleaseStopSync();
            m_socket.reset();

        });
    NX_VERBOSE(this, "%1: Destroyed.", m_sessionId);
}

void Srflx::onBytesRead(SystemError::ErrorCode errorCode, std::size_t bytesTransferred)
{
    nx::utils::Guard doPutResult(
        [this]()
        {
            putResult(m_mappedAddress);
        });

    if (nx::network::socketCannotRecoverFromError(errorCode) || bytesTransferred == 0)
        return;

    stun::Message message;
    stun::MessageParser stunParser;
    stunParser.setMessage(&message);
    size_t parsed = 0;
    if (stunParser.parse(m_readBuffer, &parsed) != nx::network::server::ParserState::done)
    {
        NX_VERBOSE(this, "%1: STUN message not parsed", m_sessionId);
        return;
    }

    if (message.header.messageClass != stun::MessageClass::successResponse
        || message.header.method != stun::MethodType::bindingMethod)
    {
        NX_VERBOSE(this, "%1: STUN binding message wrong header", m_sessionId);
        return;
    }

    auto xorMappedAddress = message.getAttribute<stun::attrs::XorMappedAddress>();
    if (!xorMappedAddress)
    {
        NX_DEBUG(this, "%1: STUN no XOR-MAPPED-ADDRESS attribute", m_sessionId);
        return;
    }

    if (m_mappedAddress.isNull())
    {
        m_mappedAddress = xorMappedAddress->addr();
        NX_DEBUG(this, "%1: STUN punched reflexive address %2", m_sessionId, m_mappedAddress.toString());
    }
    else if (m_mappedAddress != xorMappedAddress->addr())
    {
        NX_DEBUG(
            this,
            "%1: STUN mapped address mismatch, can't grant passive UDP connection: %2 vs %3",
            m_sessionId,
            m_mappedAddress.toString(),
            xorMappedAddress->addr().toString());

        return;
    }
    else
    {
        NX_DEBUG(
            this,
            "%1: STUN successfully punched reflexive address %2",
            m_sessionId,
            m_mappedAddress.toString());
        return;
    }

    if (m_timeout.hasExpired(kPunchTimeout))
    {
        NX_DEBUG(this, "%1: Srflx probably (by timeout) punched reflexive address %2",
            m_sessionId, m_mappedAddress.toString());
        return;
    }

    m_readBuffer.resize(0);

    doPutResult.disarm();
    m_socket->readSomeAsync(
        &m_readBuffer,
        [this](auto... args)
        {
            onBytesRead(std::move(args)...);
        });
}

void Srflx::putResult(const nx::network::SocketAddress& socketAddress)
{
    m_handler(socketAddress, std::move(m_socket));
}

} // namespace nx::webrtc
