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

    DeviceType getDeviceType() const;

    QString toString() const;

    QnStreamDataProvider* getDeviceStreamConnection();

    virtual bool unknownDevice() const;
    QnNetworkResourcePtr updateDevice();

};




#endif //avigilon_h_16_43_h