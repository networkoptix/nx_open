#pragma once

#ifdef ENABLE_PULSE_CAMERA

#include <nx/mediaserver/resource/camera.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/streaming/media_data_packet.h>

class QnPlPulseResource : public nx::mediaserver::resource::Camera
{
public:
    static const QString MANUFACTURE;

    QnPlPulseResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames



protected:

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCroppingPhysical(QRect cropping);

    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
};

#endif // #ifdef ENABLE_PULSE_CAMERA
