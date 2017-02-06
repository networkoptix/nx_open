////////////////////////////////////////////////////////////
// 18 jun 2014    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef DATA_ONLY_CAMERA_RESOURCE_H
#define DATA_ONLY_CAMERA_RESOURCE_H

#include <core/resource/camera_resource.h>

// TODO: #AK does not belong in /resource_management, move to /resource
// Use Qn prefix?

//!Camera resource of type, not supported on this server
/*!
    Contains only resource's data. Used primarily on edge servers, since all vendors supported is disabled with preprocessor macro
*/
class DataOnlyCameraResource
:
    public QnVirtualCameraResource
{
public:
    DataOnlyCameraResource( const QnUuid& resourceTypeId );

    //!Implementation of QnSecurityCamResource::getDriverName
    virtual QString getDriverName() const override;
    //!Implementation of QnSecurityCamResource::setIframeDistance
    virtual void setIframeDistance(int frames, int timems) override;

protected:
    //!Implementation of QnSecurityCamResource::createLiveDataProvider
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
};

#endif //DATA_ONLY_CAMERA_RESOURCE_H
