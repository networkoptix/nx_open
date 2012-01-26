#ifndef _camera_resource
#define _camera_resource

#include "network_resource.h"
#include "security_cam_resource.h"

class QN_EXPORT QnVirtualCameraResource : virtual public QnNetworkResource, virtual public QnSecurityCamResource
{
    Q_OBJECT;

public:
    QnCameraResource() {}

};

typedef QSharedPointer<QnVirtualCameraResource> QnVirtualCameraResourcePtr;
typedef QList<QnVirtualCameraResourcePtr> QnVirtualCameraResourceList;

class QN_EXPORT QnPhysicalCameraResource : virtual public QnVirtualCameraResource
{
};

typedef QSharedPointer<QnPhysicalCameraResource> QnPhysicalCameraResourcePtr;
typedef QList<QnPhysicalCameraResourcePtr> QnPhysicalCameraResourceList;

#endif // _camera_resource
