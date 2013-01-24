////////////////////////////////////////////////////////////
// 25 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "ondemand_media_data_provider.h"


OnDemandMediaDataProvider::OnDemandMediaDataProvider( const QSharedPointer<QnAbstractStreamDataProvider>& dataProvider ) throw()
{
}

//!Implementation of AbstractOnDemandDataProvider::tryRead
bool OnDemandMediaDataProvider::tryRead( QnAbstractDataPacketPtr* const data )
{
    //TODO/IMPL
    return false;
}
