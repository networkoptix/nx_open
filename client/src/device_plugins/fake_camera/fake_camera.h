#ifndef _FAKE_CAMERA_H__
#define _FAKE_CAMERA_H__

#include "core/resource/camera_resource.h"
#include "core/resource/media_resource.h"




class QnFakeCamera: public QnCameraResource
{
public:
    QnFakeCamera();

    virtual bool isResourceAccessible()  { return true; }
    virtual bool updateMACAddress()   { return true; }

    virtual QnAbstractStreamDataProvider* createDataProvider(ConnectionRole role);
};
typedef QSharedPointer<QnFakeCamera> QnFakeCameraPtr;

#endif // _FAKE_CAMERA_H__
