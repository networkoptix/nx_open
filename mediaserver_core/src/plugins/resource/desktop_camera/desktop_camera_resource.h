#pragma once

#ifdef ENABLE_DESKTOP_CAMERA

#include <QMap>
#include <nx/utils/thread/mutex.h>

#include <nx/mediaserver/resource/camera.h>

class QnDesktopCameraResource:
    public nx::mediaserver::resource::Camera
{
    Q_OBJECT

    typedef nx::mediaserver::resource::Camera base_type;

public:
    static const QString MANUFACTURE;

    static const int MAX_STREAMS = 2;

    QnDesktopCameraResource();
    QnDesktopCameraResource(const QString &userName);
    ~QnDesktopCameraResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems) override { Q_UNUSED(frames) Q_UNUSED(timems) }

    virtual bool setRelayOutputState(const QString& outputID, bool activate, unsigned int autoResetTimeoutMS = 0) override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const override;

    virtual bool mergeResourcesIfNeeded(const QnNetworkResourcePtr &source) override;

    virtual bool isReadyToDetach() const override;

    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        bool primaryStream) override;
    virtual bool isInitialized() const override { return true; }
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
};

#endif //ENABLE_DESKTOP_CAMERA
