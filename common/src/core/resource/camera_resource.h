#ifndef _camera_resource
#define _camera_resource

#include "network_resource.h"
#include "security_cam_resource.h"

class QN_EXPORT QnVirtualCameraResource : virtual public QnNetworkResource, virtual public QnSecurityCamResource
{
    Q_OBJECT

public:
    QnVirtualCameraResource() {}
};

typedef QSharedPointer<QnVirtualCameraResource> QnVirtualCameraResourcePtr;
typedef QList<QnVirtualCameraResourcePtr> QnVirtualCameraResourceList;

class QN_EXPORT QnPhysicalCameraResource : virtual public QnVirtualCameraResource
{
    Q_OBJECT
public:
    // returns 0 if primary stream does not exist
    int getPrimaryStreamDesiredFps() const;

    // the difference between desired and real is that camera can have multiple clients we do not know about or big exposure time
    int getPrimaryStreamRealFps() const;

    void onPrimaryFpsUpdated(int newFps);

#ifdef _DEBUG
    void debugCheck() const;
#endif


};

typedef QSharedPointer<QnPhysicalCameraResource> QnPhysicalCameraResourcePtr;
typedef QList<QnPhysicalCameraResourcePtr> QnPhysicalCameraResourceList;

#endif // _camera_resource
