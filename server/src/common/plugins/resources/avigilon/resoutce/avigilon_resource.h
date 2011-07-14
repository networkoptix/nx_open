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

    QString toString() const;

    QnMediaStreamDataProvider* getDeviceStreamConnection();

    virtual bool unknownResource() const;
    QnNetworkResourcePtr updateResource();

};




#endif //avigilon_h_16_43_h