#ifndef brickcom_resource_h_2049
#define brickcom_resource_h_2049

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"

class QnPlBrickcomResource : public QnPhysicalCameraResource
{
public:
    static const char* MANUFACTURE;

    QnPlBrickcomResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    bool hasDualStreaming() const override {return false;}

protected:

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCropingPhysical(QRect croping);
};

#endif //brickcom_resource_h_2049
