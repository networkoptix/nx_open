#ifndef QN_FAKE_CAMERA_H
#define QN_FAKE_CAMERA_H

#include "core/resource/camera_resource.h"
#include "core/resource/media_resource.h"

class QnFakeCamera: public QnVirtualCameraResource
{
public:
    QnFakeCamera();

    virtual bool isResourceAccessible()  { return true; }
    virtual bool updateMACAddress()   { return true; }

    virtual QnAbstractStreamDataProvider* createDataProvider(Qn::ConnectionRole role);
};

typedef QnSharedResourcePointer<QnFakeCamera> QnFakeCameraPtr;
typedef QList<QnFakeCameraPtr> QnFakeCameraList;

#endif // QN_FAKE_CAMERA_H
