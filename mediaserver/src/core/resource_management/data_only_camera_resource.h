////////////////////////////////////////////////////////////
// 18 jun 2014    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef DATA_ONLY_CAMERA_RESOURCE_H
#define DATA_ONLY_CAMERA_RESOURCE_H

#include <core/resource/camera_resource.h>


//!Camera resource of type, not supported on this server
/*!
    Contains only resource's data. Used primarily on edge servers, since all vendors supported is disabled with preprocessor macro
*/
class DataOnlyCameraResource
:
    public QnVirtualCameraResource
{
public:
    DataOnlyCameraResource( const QnId& resourceTypeId );

    //!Implementation of ::isResourceAccessible
    virtual bool QnNetworkResource::isResourceAccessible() override;
    //!Implementation of ::getDriverName
    virtual QString QnSecurityCamResource::getDriverName() const override;
    //!Implementation of ::setIframeDistance
    virtual void QnSecurityCamResource::setIframeDistance(int frames, int timems) override;

protected:
    //!Implementation of ::createLiveDataProvider
    virtual QnAbstractStreamDataProvider* QnSecurityCamResource::createLiveDataProvider() override;
};

#endif //DATA_ONLY_CAMERA_RESOURCE_H
