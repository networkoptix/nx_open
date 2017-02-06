////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "abstract_media_data_filter.h"

#include <nx/utils/log/assert.h>

#ifdef ENABLE_DATA_PROVIDERS

AbstractMediaDataFilter::AbstractMediaDataFilter( const AbstractOnDemandDataProviderPtr& dataSource )
:
    m_dataSource( dataSource )
{
    NX_ASSERT( m_dataSource );
    connect( dataSource.get(), &AbstractOnDemandDataProvider::dataAvailable,
             this, [this]( AbstractOnDemandDataProvider* /*pThis*/ ) { emit dataAvailable(this); },
             Qt::DirectConnection );
}

bool AbstractMediaDataFilter::tryRead( QnAbstractDataPacketPtr* const data )
{
    if( !m_dataSource->tryRead( data ) )
        return false;
    if( !data )
        return true;
    *data = processData( data );
    return true;
}

//!Implementation of AbstractOnDemandDataProvider::currentPos
quint64 AbstractMediaDataFilter::currentPos() const
{
    return m_dataSource->currentPos();
}

void AbstractMediaDataFilter::put(QnAbstractDataPacketPtr packet)
{
    return m_dataSource->put(std::move(packet));
}

#endif // ENABLE_DATA_PROVIDERS
