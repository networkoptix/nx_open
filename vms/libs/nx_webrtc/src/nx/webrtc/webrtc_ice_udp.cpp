// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_ice_udp.h"

#include <nx/network/socket_factory.h>
#include <nx/utils/log/log.h>

namespace nx::webrtc {

constexpr size_t kRawReadBufferSize = nx::network::AbstractDatagramSocket::MAX_DATAGRAM_SIZE;
constexpr std::chrono::seconds kKeepAliveTimeout{15};
constexpr std::chrono::milliseconds kBindingTimeout{50};
constexpr int kBindingCountLimit = 100; // Send binding for first 5 seconds.

using namespace nx::network;

IceUdp::IceUdp(SessionPool* sessionPool, const SessionConfig& config)
    :
    Ice(sessionPool, config)
{
    m_readBuffer.reserve(kRawReadBufferSize);
    m_keepAliveTimer.bindToAioThread(m_pollable.getAioThread());
    m_bindingTimer.bindToAioThread(m_pollable.getAioThread());
}

IceUdp::~IceUdp()
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

void IceUdp::stopUnsafe()
{
    if (!m_needStop)
    {
        m_keepAliveTimer.cancelSync();
        m_bindingTimer.cancelSync();
        m_socket->cancelIOSync(aio::EventType::etAll);

        Ice::stopUnsafe(); //< Dangerous, object can be destroyed after this call.
    }
}

IceCandidate::Filter IceUdp::type() const
{
    return m_bindingCount == 0 ? IceCandidate::Filter::UdpHost : IceCandidate::Filter::UdpSrflx;
}

void IceUdp::resetSocket(std::unique_ptr<nx::network::AbstractDatagramSocket>&& socket)
{
    m_socket = std::move(socket);
    NX_VERBOSE(this, "Running ice: srflx to %1", iceLocalAddress().toString());

    m_socket->bindToAioThread(m_pollable.getAioThread());

    m_socket->readSomeAsync(
        &m_readBuffer,
        [this](auto... args)
        {
            onBytesRead(std::move(args)...);
        });
}

std::optional<nx::network::SocketAddress> IceUdp::bind(const nx::network::HostAddress& host)
{
    m_socket = nx::network::SocketFactory::createDatagramSocket();
    m_socket->bindToAioThread(m_pollable.getAioThread());
    m_socket->setNonBlockingMode(true);
    if (!m_socket->bind(nx::network::SocketAddress(host, 0)))
        return std::nullopt;

    NX_VERBOSE(this, "Running ice: binded to %1", iceLocalAddress().toString());

    m_socket->readSomeAsync(
        &m_readBuffer,
        [this](auto... args)
        {
            onBytesRead(std::move(args)...);
        });
    return iceLocalAddress();
}

void IceUdp::sendBinding(
    const IceCandidate& iceCandidate,
    const SessionDescription& sessionDescription)
{
    NX_CRITICAL(m_socket);
    m_socket->dispatch(
        [this, iceCandidate, sessionDescription]
        {
            if (m_stage != Stage::binding || ++m_bindingCount > kBindingCountLimit)
                return;
            if (iceCandidate.address.isNull() || iceCandidate.address.address.isPureIpV6())
            {
                // TODO Support for ipv6 srflx too.
                NX_DEBUG(
                    this,
                    "STUN: %1: Got wrong or IpV6 address from remote peer (%2), ignoring",
                    sessionDescription.id,
                    iceCandidate.address);
                return;
            }

            m_firstPacket = false;
            m_socket->setDestAddr(iceCandidate.address);

            stun::Message request(stun::Header(
                stun::MessageClass::request, stun::MethodType::bindingMethod));
            request.newAttribute<stun::attrs::Priority>(iceCandidate.priority);
            std::string stunUsername(sessionDescription.remoteUfrag + ":" + sessionDescription.id);
            /* Actually, valid integrity should be signed with remote pwd.
             * But, if this request succeeded, we also need to negotiate DTLS handshake role.
             * */
            request.insertIntegrity(stunUsername, sessionDescription.localPwd);

            stun::MessageSerializer stunSerializer;
            stunSerializer.setAlwaysAddFingerprint(true);
            nx::Buffer sendBuffer = stunSerializer.serialized(request);
            writePacket(sendBuffer.data(), sendBuffer.size(), /*foreground*/ true);

            NX_VERBOSE(this, "STUN: binding sent for session %1", sessionDescription.id);

            m_bindingTimer.start(
                kBindingTimeout,
                [this, iceCandidate, sessionDescription]()
                {
                    sendBinding(iceCandidate, sessionDescription);
                });
        });
}

void IceUdp::onBytesRead(SystemError::ErrorCode errorCode, std::size_t bytesTransferred)
{
    if (m_needStop)
        return;

    if (nx::network::socketCannotRecoverFromError(errorCode) || bytesTransferred == 0)
    {
        stopUnsafe();
    }
    else
    {
        if (m_readBuffer.size())
        {
            if (m_firstPacket)
            {
                // NOTE: for host candidate, we can receive packet only from remote host candidate.
                // For srflx candidate, we can receive packet from remote srflx or from STUN server.
                // We set right address for srflx in sendBinding().
                m_socket->setDestAddr(m_socket->lastDatagramSourceAddress());
            }

            bool result = processIncomingPacket((uint8_t*) m_readBuffer.data(), m_readBuffer.size());
            if (m_needStop) // processIncomingPacket can call stopUnsafe on write data.
                return;
            m_readBuffer.resize(0);
            if (!result && m_stage != Stage::binding)
            {
                stopUnsafe();
                return;
            }

            if (result)
            {
                m_firstPacket = false;
                m_keepAliveTimer.start(
                    kKeepAliveTimeout,
                    [this]()
                    {
                        NX_DEBUG(
                            this,
                            "Keep alive timeout reached, stopping UDP ice for %1",
                            (m_sessionDescription ? m_sessionDescription->id : "(null)"));
                        stopUnsafe(); //< Ice restart is better.
                    });
            }
        }

        m_socket->readSomeAsync(
            &m_readBuffer,
            [this](auto... args)
            {
                onBytesRead(std::move(args)...);
            });
    }
};

void IceUdp::asyncSendPacketUnsafe()
{
    m_socket->post(
        [this]()
        {
            if (m_needStop)
                return;
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_socket->sendAsync(
                &m_sendBuffers.front(),
                [this](auto... args)
                {
                    onBytesWritten(std::move(args)...);
                });
        });
}

nx::network::SocketAddress IceUdp::iceRemoteAddress() const
{
    return m_socket->lastDatagramSourceAddress();
}

nx::network::SocketAddress IceUdp::iceLocalAddress() const
{
    return m_socket->getLocalAddress();
}

} // namespace nx::webrtc
