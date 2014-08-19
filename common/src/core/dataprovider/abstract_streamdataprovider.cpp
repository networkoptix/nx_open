#include "abstract_streamdataprovider.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "../resource/resource.h"

QnAbstractStreamDataProvider::QnAbstractStreamDataProvider(const QnResourcePtr& resource):
    QnResourceConsumer(resource),
    m_mutex(QMutex::Recursive),
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
    QMutexLocker mutex(&m_mutex);
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractDataReceptor* dp = m_dataprocessors.at(i);
        if (!dp->canAcceptData())
            return false;
    }
    return true;
}

int QnAbstractStreamDataProvider::processorsCount() const
{
    QMutexLocker mutex(&m_mutex);
    return m_dataprocessors.size();
}

void QnAbstractStreamDataProvider::addDataProcessor(QnAbstractDataReceptor* dp)
{
    QMutexLocker mutex(&m_mutex);

    if (!m_dataprocessors.contains(dp))
    {
        m_dataprocessors.push_back(dp);


    }
}

void QnAbstractStreamDataProvider::removeDataProcessor(QnAbstractDataReceptor* dp)
{
    QMutexLocker mutex(&m_mutex);
    m_dataprocessors.removeOne(dp);
}

void QnAbstractStreamDataProvider::putData(const QnAbstractDataPacketPtr& data)
{
    if (!data)
        return;

    QMutexLocker mutex(&m_mutex);
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractDataReceptor* dp = m_dataprocessors.at(i);
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

#endif // ENABLE_DATA_PROVIDERS
