#pragma once

#include <nx/vms/server/resource/camera.h>

namespace nx::vms::server::resource {

/**
 * Camera resource of type, not supported on this server.
 * Contains only resource's data. Used primarily on edge servers, since all vendors supported is
 * disabled with preprocessor macro.
 */
class DataOnlyCamera: public Camera
{
public:
    DataOnlyCamera(QnMediaServerModule* serverModule, const QnUuid& resourceTypeId);

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual QString getDriverName() const override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
};

} // namespace nx::vms::server::resource
