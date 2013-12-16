#ifndef pulse_resource_h_1947
#define pulse_resource_h_1947

#ifdef ENABLE_PULSE_CAMERA

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"

class QnPlPulseResource : public QnPhysicalCameraResource
{
public:
    static const char* MANUFACTURE;

    QnPlPulseResource();

    virtual bool isResourceAccessible();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    

protected:

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCroppingPhysical(QRect cropping);
};

#endif // #ifdef ENABLE_PULSE_CAMERA
#endif //pulse_resource_h_1947
