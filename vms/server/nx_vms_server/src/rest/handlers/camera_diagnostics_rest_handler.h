#pragma once

#include <core/resource/resource_fwd.h>
#include <utils/camera/camera_diagnostics.h>
#include <rest/server/json_rest_handler.h>

#include "camera/video_camera.h"

#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/server_module_aware.h>

class QnCameraDiagnosticsRestHandler:
    public QnJsonRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    QnCameraDiagnosticsRestHandler(QnMediaServerModule* serverModule);
    virtual QStringList cameraIdUrlParams() const override;

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

private:
    static CameraDiagnostics::Result checkCameraAvailability(
        const nx::vms::server::resource::CameraPtr& cameraRes);

    static CameraDiagnostics::Result tryAcquireCameraMediaStream(
        const QnVideoCameraPtr& videoCamera);

    static CameraDiagnostics::Result checkCameraMediaStreamForErrors(
        const nx::vms::server::resource::CameraPtr& camera);
};
