#include "abstract_stream_data_provider.h"

#include <core/resource/resource.h>
#include <core/dataconsumer/abstract_data_receptor.h>

QnAbstractStreamDataProvider::QnAbstractStreamDataProvider(const QnResourcePtr& resource):
    QnResourceConsumer(resource),
    m_mutex(QnMutex::Recursive),
    m_role(Qn::CR_Default)
{
}

QnAbstractStreamDataProvider::~QnAbstractStreamDataProvider()
{
    stop();
}

bool QnAbstractStreamDataProvider::dataCanBeAccepted() const
{
    // need to read only if all queues has more space and at least one queue is exist
    QnMutexLocker mutex( &m_mutex );
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractMediaDataReceptor* dp = m_dataprocessors.at(i);
        if (!dp->canAcceptData())
            return false;
    }
    return true;
}

int QnAbstractStreamDataProvider::processorsCount() const
{
    QnMutexLocker mutex( &m_mutex );
    return m_dataprocessors.size();
}

void QnAbstractStreamDataProvider::addDataProcessor(QnAbstractMediaDataReceptor* dp)
{
    NX_ASSERT(dp);
    QnMutexLocker mutex( &m_mutex );

    if (!m_dataprocessors.contains(dp))
    {
        m_dataprocessors.push_back(dp);


    }
}

bool QnAbstractStreamDataProvider::needConfigureProvider() const
{
    QnMutexLocker mutex( &m_mutex );
    for (auto processor: m_dataprocessors)
    {
        if (processor->needConfigureProvider())
            return true;
    }
    return false;
}

void QnAbstractStreamDataProvider::removeDataProcessor(QnAbstractMediaDataReceptor* dp)
{
    QnMutexLocker mutex( &m_mutex );
    m_dataprocessors.removeOne(dp);
}

void QnAbstractStreamDataProvider::putData(const QnAbstractDataPacketPtr& data)
{
    if (!data)
        return;

    QnMutexLocker mutex( &m_mutex );
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractMediaDataReceptor* dp = m_dataprocessors.at(i);
        dp->putData(data);
    }
}

bool QnAbstractStreamDataProvider::isConnectedToTheResource() const
{
    return m_resource->hasConsumer(const_cast<QnAbstractStreamDataProvider*>(this));
}

void QnAbstractStreamDataProvider::disconnectFromResource()
{
    stop();

    QnResourceConsumer::disconnectFromResource();
}

void QnAbstractStreamDataProvider::beforeDisconnectFromResource()
{
    pleaseStop();
}

void QnAbstractStreamDataProvider::setRole(Qn::ConnectionRole role)
{
    m_role = role;
}
