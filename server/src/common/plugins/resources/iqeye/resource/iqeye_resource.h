#ifndef iqeye_h_2054_h
#define iqeye_h_2054_h

#include "resource/network_resource.h"

class QnPlQEyeResource;
typedef QSharedPointer<QnPlQEyeResource> QnPlQEyeResourcePtr;

class QnPlQEyeResource : public QnNetworkResource
{
public:
    QnPlQEyeResource();
    ~QnPlQEyeResource();

    QString toString() const;
    QnMediaStreamDataProvider* getDeviceStreamConnection();

    virtual bool unknownResource() const;
    QnNetworkResourcePtr updateResource();

};




#endif //avigilon_h_16_43_h