////////////////////////////////////////////////////////////
// 25 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "ondemand_media_data_provider.h"

#include <utils/common/mutex.h>


static const size_t MAX_DATA_QUEUE_SIZE = 16;   //TODO: #ak is queue really needed here?

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
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    if( m_dataQueue.empty() )
        return false;

    *data = m_dataQueue.front();
    m_prevPacketTimestamp = m_dataQueue.front()->timestamp;
    m_dataQueue.pop();
    return true;
}

quint64 OnDemandMediaDataProvider::currentPos() const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    if( m_prevPacketTimestamp != -1 )
        return m_prevPacketTimestamp;
    if( !m_dataQueue.empty() )
        return m_dataQueue.front()->timestamp;
    return -1;
}

bool OnDemandMediaDataProvider::canAcceptData() const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    return m_dataQueue.size() < MAX_DATA_QUEUE_SIZE;
}

void OnDemandMediaDataProvider::putData( const QnAbstractDataPacketPtr& data )
{
    bool dataBecameAvailable = false;
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        dataBecameAvailable = m_dataQueue.empty();
        m_dataQueue.push( data );
    }

    if( dataBecameAvailable )
        emit dataAvailable( this );
}
