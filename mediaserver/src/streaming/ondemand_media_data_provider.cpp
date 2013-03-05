////////////////////////////////////////////////////////////
// 25 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "ondemand_media_data_provider.h"

#include <QMutexLocker>


static const size_t MAX_DATA_QUEUE_SIZE = 16;

OnDemandMediaDataProvider::OnDemandMediaDataProvider( const QSharedPointer<QnAbstractStreamDataProvider>& dataProvider ) throw()
:
    m_dataProvider( dataProvider ),
    m_prevPacketTimestamp( 0 )
{
}

//!Implementation of AbstractOnDemandDataProvider::tryRead
bool OnDemandMediaDataProvider::tryRead( QnAbstractDataPacketPtr* const data )
{
    QMutexLocker lk( &m_mutex );

    if( m_dataQueue.empty() )
        return false;

    *data = m_dataQueue.front();
    m_prevPacketTimestamp = m_dataQueue.front()->timestamp;
    m_dataQueue.pop();
    return true;
}

quint64 OnDemandMediaDataProvider::currentPos() const
{
    QMutexLocker lk( &m_mutex );

    //TODO/IMPL/HLS
    //Q_ASSERT( false );
    return m_prevPacketTimestamp;
}

bool OnDemandMediaDataProvider::canAcceptData() const
{
    QMutexLocker lk( &m_mutex );
    return m_dataQueue.size() < MAX_DATA_QUEUE_SIZE;
}

void OnDemandMediaDataProvider::putData( QnAbstractDataPacketPtr data )
{
    bool dataBecameAvailable = false;
    {
        QMutexLocker lk( &m_mutex );
        dataBecameAvailable = m_dataQueue.empty();
        m_dataQueue.push( data );
    }

    if( dataBecameAvailable )
        emit dataAvailable( this );
}

bool OnDemandMediaDataProvider::seek( quint64 /*positionToSeek*/ )
{
    QMutexLocker lk( &m_mutex );

    //TODO/IMPL/HLS
    //Q_ASSERT( false );
    return true;
}
