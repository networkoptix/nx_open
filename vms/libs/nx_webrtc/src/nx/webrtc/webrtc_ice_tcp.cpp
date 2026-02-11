// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_ice_tcp.h"

#include <network/tcp_connection_priv.h>

#include "webrtc_session_pool.h"

namespace nx::webrtc {

constexpr size_t kRfc4571HeaderSize = 2;
constexpr size_t kRawReadBufferSize = 65536;

using namespace nx::network;

ListenerTcp::ListenerTcp(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner,
    SessionPool* sessionPool,
    int64_t maxTcpRequestSize)
    :
    QnTCPConnectionProcessor(
        std::move(socket), owner, maxTcpRequestSize),
    m_sessionPool(sessionPool)
{
    setObjectName(nx::toString(this));
}

void ListenerTcp::run()
{
    Q_D(QnTCPConnectionProcessor);
    initSystemThreadId();
    NX_VERBOSE(this, "Running ice: request size = %1", d->clientRequest.size());

    auto ice = std::make_unique<IceTcp>(
        takeSocket(),
        m_sessionPool,
        SessionConfig(),
        d->clientRequest);

    m_sessionPool->gotIceTcp(std::move(ice));
}

IceTcp::IceTcp(
    std::unique_ptr<nx::network::AbstractStreamSocket>&& socket,
    SessionPool* sessionPool,
    const SessionConfig& config,
    const QByteArray& readBuffer
    )
    :
    Ice(sessionPool, config),
    m_socket(std::move(socket), readBuffer)
{
    m_readBuffer.reserve(kRawReadBufferSize);

    m_socket.bindToAioThread(m_pollable.getAioThread());
    m_socket.setSendBufferSize(kIceTcpSendBufferSize);
    m_socket.setNonBlockingMode(true);
}

void IceTcp::start()
{
    m_socket.readSomeAsync(
        &m_readBuffer,
        [this](auto... args)
        {
            onBytesRead(std::move(args)...);
        });
}

IceCandidate::Filter IceTcp::type() const
{
    return IceCandidate::Filter::TcpHost;
}

IceTcp::~IceTcp()
{
    std::promise<void> stoppedPromise;
    m_pollable.dispatch(
        [this, &stoppedPromise]()
        {
            stopUnsafe();
            stoppedPromise.set_value();
        });
    stoppedPromise.get_future().wait();
    NX_VERBOSE(this, "Destroyed.");
}

void IceTcp::stopUnsafe()
{
    if (!m_needStop)
    {
        Ice::stopUnsafe();

        m_socket.cancelIOSync(aio::EventType::etAll);
    }
    m_stopped = true;
}

bool IceTcp::isStopped()
{
    return m_stopped;
}

nx::Buffer IceTcp::toSendBuffer(const char* data, int dataSize) const
{
    if (dataSize > std::numeric_limits<uint16_t>::max())
    {
        NX_DEBUG(
            this,
            "Can't send message: size is too large %1 for %2",
            dataSize,
            (m_sessionDescription ? m_sessionDescription->id : "(null)"));
        return nx::Buffer();
    }

    nx::Buffer packet;
    uint16_t size = htons((uint16_t)dataSize);
    packet.append((const char*)&size, kRfc4571HeaderSize);
    packet.append(data, dataSize);
    return packet;
}

void IceTcp::onBytesRead(SystemError::ErrorCode errorCode, std::size_t bytesTransferred)
{
    if (m_needStop)
        return;

    if (nx::network::socketCannotRecoverFromError(errorCode) || bytesTransferred == 0)
    {
        stopUnsafe();
    }
    else
    {
        bool result = true;
        while (m_readBuffer.size() >= kRfc4571HeaderSize)
        {
            char* data = m_readBuffer.data();
            uint16_t packetSize = qFromBigEndian(*(uint16_t*) data);
            if (m_readBuffer.size() >= packetSize + kRfc4571HeaderSize)
            {
                result = processIncomingPacket((uint8_t*) data + kRfc4571HeaderSize, packetSize);
                m_readBuffer.erase(0, packetSize + kRfc4571HeaderSize);
                if (!result)
                    break;
            }
            else
            {
                break;
            }
        }
        if (result)
            m_socket.readSomeAsync(
                &m_readBuffer,
                [this](auto... args)
                {
                    onBytesRead(std::move(args)...);
                });
        else
            stopUnsafe();
    }
};

void IceTcp::asyncSendPacketUnsafe()
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

nx::network::SocketAddress IceTcp::iceRemoteAddress() const
{
    return m_socket.getForeignAddress();
}

nx::network::SocketAddress IceTcp::iceLocalAddress() const
{
    return m_socket.getLocalAddress();
}

} // namespace nx::webrtc
