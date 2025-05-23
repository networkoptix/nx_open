#include "multiplexer.h"

#include "queue.h"

static constexpr int kDefaultRecvUnitQueueSize = 32;

Multiplexer::Multiplexer(
    int ipVersion,
    int maximumSegmentSize,
    bool reusable,
    int id)
    :
    ipVersion(ipVersion),
    maximumSegmentSize(maximumSegmentSize),
    reusable(reusable),
    id(id),
    m_udpChannel(UdpChannelFactory::instance().create(ipVersion)),
    m_timer(std::make_unique<CTimer>()),
    m_sendQueue(std::make_unique<CSndQueue>(m_udpChannel.get(), m_timer.get())),
    m_recvQueue(std::make_unique<CRcvQueue>(
        kDefaultRecvUnitQueueSize,
        ipVersion,
        m_udpChannel.get(),
        m_timer.get()))
{
}

Multiplexer::~Multiplexer()
{
    shutdown();
}

Result<> Multiplexer::start()
{
    if (auto result = m_sendQueue->start(); !result.ok())
        return result;

    if (auto result = m_recvQueue->start(); !result.ok())
        return result;

    return success();
}

void Multiplexer::shutdown()
{
    if (m_udpChannel)
        m_udpChannel->shutdown();
    if (m_recvQueue)
        m_recvQueue->stop();
    if (m_sendQueue)
        m_sendQueue->stop();
}

AbstractUdpChannel& Multiplexer::channel()
{
    return *m_udpChannel;
}

CSndQueue& Multiplexer::sendQueue()
{
    return *m_sendQueue;
}

CRcvQueue& Multiplexer::recvQueue()
{
    return *m_recvQueue;
}

int Multiplexer::udpPort() const
{
    return m_udpChannel->getSockAddr().port();
}
