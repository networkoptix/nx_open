#include "abstract_stream_data_provider.h"

#include <core/resource/resource.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/utils/log/log.h>

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
    NX_VERBOSE(this, "Check if data can be accepted");

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
    NX_CRITICAL(dp);

    QnMutexLocker mutex (&m_mutex);
    if (!m_dataprocessors.contains(dp))
    {
        NX_DEBUG(this, "Add data processor: %1", dp);
        m_dataprocessors.push_back(dp);
    }
    else
    {
        NX_WARNING(this, "Data processor is already added: %1", dp);
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
    QnMutexLocker mutex (&m_mutex);
    if (m_dataprocessors.removeOne(dp))
        NX_DEBUG(this, "Remove data processor: %1", dp);
    else
        NX_WARNING(this, "Remove not added data processor: %1", dp);
}

void QnAbstractStreamDataProvider::putData(const QnAbstractDataPacketPtr& data)
{
    if (!data)
        return;

    QnMutexLocker mutex( &m_mutex );
    for (const auto& p: m_dataprocessors)
        p->putData(data);
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
    NX_ASSERT(!isRunning());
    m_role = role;
}

Qn::ConnectionRole QnAbstractStreamDataProvider::getRole() const
{
    return m_role;
}
