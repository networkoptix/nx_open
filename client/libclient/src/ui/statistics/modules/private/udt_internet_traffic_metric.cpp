#include "udt_internet_traffic_metric.h"

#include <nx/network/udt/udt_socket.h>

UdtInternetTrafficMetric::UdtInternetTrafficMetric()
{
    nx::network::UdtStatistics::instance().addRecvHandler(this,
        [this](nx::network::UdtStreamSocket* socket, int bytes)
        {
            if (socket->getForeignAddress().address.isLocal())
                return;

            QnMutexLocker lock(&m_mutex);
            m_readBytes += (uint64_t) bytes;
        });

    nx::network::UdtStatistics::instance().addSendHandler(this,
        [this](nx::network::UdtStreamSocket* socket, int bytes)
        {
            if (socket->getForeignAddress().address.isLocal())
                return;

            QnMutexLocker lock(&m_mutex);
            m_writeBytes += (uint64_t) bytes;
        });
}

UdtInternetTrafficMetric::~UdtInternetTrafficMetric()
{
    nx::network::UdtStatistics::instance().removeHandlers(this);
}

bool UdtInternetTrafficMetric::isSignificant() const
{
    QnMutexLocker lock(&m_mutex);
    return (m_readBytes != 0) || (m_writeBytes != 0);
}

QString UdtInternetTrafficMetric::value() const
{
    QnMutexLocker lock(&m_mutex);
    return lm("%1/%2").strs(m_readBytes, m_writeBytes);
}

void UdtInternetTrafficMetric::reset()
{
    QnMutexLocker lock(&m_mutex);
    m_readBytes = 0;
    m_writeBytes = 0;
}
