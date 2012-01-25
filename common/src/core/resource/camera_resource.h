#ifndef _camera_resource
#define _camera_resource

#include "network_resource.h"
#include "security_cam_resource.h"

class QnCameraResource;
typedef QSharedPointer<QnCameraResource> QnCameraResourcePtr;
typedef QList<QnCameraResourcePtr> QnCameraResourceList;

class QN_EXPORT QnCameraResource : virtual public QnNetworkResource, virtual public QnSecurityCamResource
{
    Q_OBJECT;

public:
    QnCameraResource() {}

};

#endif // _camera_resource
