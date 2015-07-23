#ifndef ipwebcam_droid_resource_h_1517
#define ipwebcam_droid_resource_h_1517

#ifdef ENABLE_DROID

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"

static const int DROID_WEB_CAM_PORT = 8089;

class QnPlDriodIpWebCamResource : public QnPhysicalCameraResource
{
public:
    static const QString MANUFACTURE;

    QnPlDriodIpWebCamResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    int httpPort() const override;


protected:

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCroppingPhysical(QRect cropping);
};

#endif // #ifdef ENABLE_DROID
#endif //ipwebcam_droid_resource_h_1517
