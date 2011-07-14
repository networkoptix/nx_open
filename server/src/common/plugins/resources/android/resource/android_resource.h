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

    QnMediaStreamDataProvider* getDeviceStreamConnection();

    virtual bool unknownResource() const;
    QnNetworkResourcePtr updateResource();

};

typedef QSharedPointer<QnPlANdroidResource> QnPlANdroidResourcePtr;

#endif //avigilon_h_16_43_h