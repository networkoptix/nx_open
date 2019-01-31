#include "ondemand_media_data_provider.h"

#include <nx/utils/thread/mutex.h>


static const size_t MAX_DATA_QUEUE_SIZE = 16;

OnDemandMediaDataProvider::OnDemandMediaDataProvider( const QSharedPointer<QnAbstractStreamDataProvider>& dataProvider ) throw()
:
    m_dataProvider( dataProvider ),
    m_prevPacketTimestamp( -1 )
{
    m_dataProvider->addDataProcessor( this );
}

OnDemandMediaDataProvider::~OnDemandMediaDataProvider() throw()
{
    m_dataProvider->removeDataProcessor( this );
}

//!Implementation of AbstractOnDemandDataProvider::tryRead
bool OnDemandMediaDataProvider::tryRead( QnAbstractDataPacketPtr* const data )
{
    QnMutexLocker lk( &m_mutex );

    if( m_dataQueue.empty() )
        return false;

    *data = m_dataQueue.front();
    m_prevPacketTimestamp = m_dataQueue.front()->timestamp;
    m_dataQueue.pop_front();
    return true;
}

quint64 OnDemandMediaDataProvider::currentPos() const
{
    QnMutexLocker lk( &m_mutex );

    if( m_prevPacketTimestamp != -1 )
        return m_prevPacketTimestamp;
    if( !m_dataQueue.empty() )
        return m_dataQueue.front()->timestamp;
    return -1;
}

void OnDemandMediaDataProvider::put(QnAbstractDataPacketPtr packet)
{
    QnMutexLocker lk(&m_mutex);
    m_dataQueue.push_front(std::move(packet));
}

bool OnDemandMediaDataProvider::canAcceptData() const
{
    QnMutexLocker lk( &m_mutex );
    return m_dataQueue.size() < MAX_DATA_QUEUE_SIZE;
}

void OnDemandMediaDataProvider::putData( const QnAbstractDataPacketPtr& data )
{
    bool dataBecameAvailable = false;
    {
        QnMutexLocker lk( &m_mutex );
        dataBecameAvailable = m_dataQueue.empty();
        m_dataQueue.push_back( data );
    }

    if( dataBecameAvailable )
        emit dataAvailable( this );
}
