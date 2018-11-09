#include "multiplexer.h"

#include "queue.h"

Multiplexer::Multiplexer(
    int ipVersion,
    int payloadSize)
    :
    m_pChannel(std::make_shared<CChannel>(ipVersion)),
    m_pTimer(std::make_shared<CTimer>()),
    m_pSndQueue(std::make_shared<CSndQueue>(m_pChannel.get(), m_pTimer.get())),
    m_pRcvQueue(std::make_shared<CRcvQueue>(
        32,
        payloadSize,
        ipVersion,
        1024,
        m_pChannel.get(),
        m_pTimer.get())),
    m_iIPversion(ipVersion)
{
}

Multiplexer::~Multiplexer()
{
    shutdown();
}

void Multiplexer::start()
{
    m_pSndQueue->start();
    m_pRcvQueue->start();
}

void Multiplexer::shutdown()
{
    if (m_pChannel)
        m_pChannel->shutdown();
    if (m_pRcvQueue)
        m_pRcvQueue->stop();
    m_pSndQueue.reset();
    m_pRcvQueue.reset();
    m_pTimer.reset();
    m_pChannel.reset();
}
