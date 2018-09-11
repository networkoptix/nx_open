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

    QnDesktopCameraResource(QnMediaServerModule* serverModule);
    QnDesktopCameraResource(QnMediaServerModule* serverModule, const QString &userName);
    ~QnDesktopCameraResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int /*frames*/, int /*timems*/) override {}

    virtual bool setRelayOutputState(const QString& outputID, bool activate,
        unsigned int autoResetTimeoutMS = 0) override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual bool mergeResourcesIfNeeded(const QnNetworkResourcePtr &source) override;

    virtual bool isReadyToDetach() const override;

    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual bool isInitialized() const override { return true; }
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual bool needCheckIpConflicts() const override { return false; }
};

#endif //ENABLE_DESKTOP_CAMERA
