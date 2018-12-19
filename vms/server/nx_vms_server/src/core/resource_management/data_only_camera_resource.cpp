////////////////////////////////////////////////////////////
// 18 jun 2014    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "data_only_camera_resource.h"


DataOnlyCameraResource::DataOnlyCameraResource( const QnUuid& resourceTypeId )
{
    setTypeId( resourceTypeId );
}

QnAbstractStreamDataProvider* DataOnlyCameraResource::createLiveDataProvider()
{
    return nullptr;
}
