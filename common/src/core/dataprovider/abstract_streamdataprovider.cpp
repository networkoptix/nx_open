#include "abstract_streamdataprovider.h"

#include "../resource/resource.h"

QnAbstractStreamDataProvider::QnAbstractStreamDataProvider(QnResourcePtr resource):
    QnResourceConsumer(resource),
    m_mutex(QMutex::Recursive)
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
        QnAbstractDataConsumer* dp = m_dataprocessors.at(i);
        if (!dp->canAcceptData())
            return false;
    }
    return true;
}


void QnAbstractStreamDataProvider::addDataProcessor(QnAbstractDataConsumer* dp)
{
    QMutexLocker mutex(&m_mutex);

    if (!m_dataprocessors.contains(dp))
    {
        m_dataprocessors.push_back(dp);


    }
}

void QnAbstractStreamDataProvider::removeDataProcessor(QnAbstractDataConsumer* dp)
{
    QMutexLocker mutex(&m_mutex);
    m_dataprocessors.removeOne(dp);
}

void QnAbstractStreamDataProvider::putData(QnAbstractDataPacketPtr data)
{
    if (!data)
        return;

    QMutexLocker mutex(&m_mutex);
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractDataConsumer* dp = m_dataprocessors.at(i);
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
