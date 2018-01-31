#ifndef ipwebcam_droid_resource_h_1517
#define ipwebcam_droid_resource_h_1517

#ifdef ENABLE_DROID

#include <nx/mediaserver/resource/camera.h>
#include <nx/network/simple_http_client.h>
#include <nx/streaming/media_data_packet.h>

static const int DROID_WEB_CAM_PORT = 8089;

class QnPlDriodIpWebCamResource: public nx::mediaserver::resource::Camera
{
public:
    static const QString MANUFACTURE;

    QnPlDriodIpWebCamResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    int httpPort() const override;


protected:
    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual void setCroppingPhysical(QRect cropping);
};

#endif // #ifdef ENABLE_DROID
#endif //ipwebcam_droid_resource_h_1517
