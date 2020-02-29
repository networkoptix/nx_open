#include "multiplexer.h"

#include "queue.h"

Multiplexer::Multiplexer(
    int ipVersion,
    int payloadSize,
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
        32,
        payloadSize,
        ipVersion,
        m_udpChannel.get(),
        m_timer.get()))
{
}

Multiplexer::~Multiplexer()
{
    shutdown();
}

void Multiplexer::start()
{
    m_sendQueue->start();
    m_recvQueue->start();
}

void Multiplexer::shutdown()
{
    if (m_udpChannel)
        m_udpChannel->shutdown();
    if (m_recvQueue)
        m_recvQueue->stop();
    m_sendQueue.reset();
    m_recvQueue.reset();
    m_timer.reset();
    m_udpChannel.reset();
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
