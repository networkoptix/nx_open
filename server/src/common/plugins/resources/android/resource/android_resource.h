#ifndef android_h_2054_h
#define android_h_2054_h

#include "resource/network_resource.h"


class QnPlANdroidResource : public QnNetworkResource
{
public:
    QnPlANdroidResource ();
    ~QnPlANdroidResource ();

    DeviceType getDeviceType() const;

    QString toString() const;

    QnStreamDataProvider* getDeviceStreamConnection();

    virtual bool unknownDevice() const;
    QnNetworkResourcePtr updateDevice();

};

typedef QSharedPointer<QnPlANdroidResource> QnPlANdroidResourcePtr;

#endif //avigilon_h_16_43_h