#ifndef _FAKE_CAMERA_H__
#define _FAKE_CAMERA_H__

#include "core/resource/network_resource.h"
#include "core/resource/media_resource.h"




class QnFakeCamera: virtual public QnNetworkResource, virtual public QnMediaResource
{
public:
    QnFakeCamera();

    virtual bool isResourceAccessible()  { return true; }
    virtual bool updateMACAddress()   { return true; }

    virtual QnAbstractStreamDataProvider* createDataProvider(ConnectionRole role);
};
typedef QSharedPointer<QnFakeCamera> QnFakeCameraPtr;

#endif // _FAKE_CAMERA_H__
