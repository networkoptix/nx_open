#ifndef pulse_resource_h_1947
#define pulse_resource_h_1947

#ifdef ENABLE_PULSE_CAMERA

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include <nx/network/deprecated/simple_http_client.h>
#include "nx/streaming/media_data_packet.h"

class QnPlPulseResource : public QnPhysicalCameraResource
{
public:
    static const QString MANUFACTURE;

    QnPlPulseResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames



protected:

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCroppingPhysical(QRect cropping);

    virtual CameraDiagnostics::Result initInternal() override;
};

#endif // #ifdef ENABLE_PULSE_CAMERA
#endif //pulse_resource_h_1947
