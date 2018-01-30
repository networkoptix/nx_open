#pragma once

#ifdef ENABLE_DROID

#include <nx/mediaserver/resource/camera.h>

class QnDroidResource: public nx::mediaserver::resource::Camera
{
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    QnDroidResource();

    virtual int getMaxFps() const override;
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override; // sets the distance between I frames


    virtual QString getHostAddress() const override;
    virtual void setHostAddress(const QString &ip) override;

protected:
    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
};

typedef QnSharedResourcePointer<QnDroidResource> QnDroidResourcePtr;

#endif // #ifdef ENABLE_DROID
