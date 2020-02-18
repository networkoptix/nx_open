#pragma once
#if defined(ENABLE_TEST_CAMERA)

#include <nx/vms/server/resource/camera.h>

class QnTestCameraResource: public nx::vms::server::resource::Camera
{
    Q_OBJECT

public:
    static constexpr const char* const kManufacturer = "NetworkOptix";
    static constexpr const char* const kModel = "TestCameraLive";

    QnTestCameraResource(QnMediaServerModule* serverModule);

    virtual int getMaxFps(StreamIndex streamIndex = StreamIndex::primary) const override;
    virtual QString getDriverName() const override;

    virtual QString getHostAddress() const override;
    virtual void setHostAddress(const QString &ip) override;
    virtual bool needCheckIpConflicts() const override { return false; }

protected:
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
};

#endif // defined(ENABLE_TEST_CAMERA)
