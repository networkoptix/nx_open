////////////////////////////////////////////////////////////
// 18 jun 2014    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "data_only_camera_resource.h"


DataOnlyCameraResource::DataOnlyCameraResource( const QnUuid& resourceTypeId )
{
    setTypeId( resourceTypeId );
}

void DataOnlyCameraResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
    //TODO
}

QnAbstractStreamDataProvider* DataOnlyCameraResource::createLiveDataProvider()
{
    return nullptr;
}
