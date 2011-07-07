#ifndef avigilon_h_16_43_h
#define avigilon_h_16_43_h

#include "resource/network_resource.h"

class QnPlAvigilonResource;
typedef QSharedPointer<QnPlAvigilonResource> QnPlAvigilonResourcePtr;

class QnPlAvigilonResource : public QnNetworkResource
{
public:
    QnPlAvigilonResource();
    ~QnPlAvigilonResource();

    DeviceType getDeviceType() const;

    QString toString() const;

    QnStreamDataProvider* getDeviceStreamConnection();

    virtual bool unknownDevice() const;
    QnNetworkResourcePtr updateDevice();

};




#endif //avigilon_h_16_43_h