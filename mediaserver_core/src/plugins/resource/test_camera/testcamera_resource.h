#pragma once

#ifdef ENABLE_TEST_CAMERA

#include <nx/mediaserver/resource/camera.h>

class QnTestCameraResource: public nx::mediaserver::resource::Camera
{
    Q_OBJECT

public:
    static constexpr const char* const kManufacturer = "NetworkOptix";
    static constexpr const char* const kModel = "TestCameraLive";

    QnTestCameraResource();

    virtual int getMaxFps() const override;
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override; // sets the distance between I frames


    virtual QString getHostAddress() const override;
    virtual void setHostAddress(const QString &ip) override;
    virtual bool needCheckIpConflicts() const override { return false; }
protected:
    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

private:
};

#endif // #ifdef ENABLE_TEST_CAMERA
