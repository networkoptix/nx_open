#ifndef ipwebcam_droid_resource_h_1517
#define ipwebcam_droid_resource_h_1517

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"

static const int DROID_WEB_CAM_PORT = 8089;

class QnPlDriodIpWebCamResource : public QnPhysicalCameraResource
{
public:
    static const char* MANUFACTURE;

    QnPlDriodIpWebCamResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    int httpPort() const override;

    bool hasDualStreaming() const override {return false;}

    bool shoudResolveConflicts() const override
    {
        return false;
    }


protected:

    void initInternal() override {}

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCropingPhysical(QRect croping);
};

#endif //ipwebcam_droid_resource_h_1517
