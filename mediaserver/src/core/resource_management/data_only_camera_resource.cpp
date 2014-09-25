////////////////////////////////////////////////////////////
// 18 jun 2014    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "data_only_camera_resource.h"


DataOnlyCameraResource::DataOnlyCameraResource( const QnUuid& resourceTypeId )
{
    setTypeId( resourceTypeId );
}

bool DataOnlyCameraResource::isResourceAccessible()
{
    return false;
}

QString DataOnlyCameraResource::getDriverName() const
{
    return lit("DATA_ONLY");
}

void DataOnlyCameraResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
    //TODO
}

QnAbstractStreamDataProvider* DataOnlyCameraResource::createLiveDataProvider()
{
    return nullptr;
}
