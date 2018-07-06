#pragma once

#include <core/resource/resource_fwd.h>
#include <utils/camera/camera_diagnostics.h>
#include <rest/server/json_rest_handler.h>

#include "camera/video_camera.h"

#include <nx/mediaserver/resource/resource_fwd.h>

class QnCameraDiagnosticsRestHandler: public QnJsonRestHandler
{
    Q_OBJECT

public:
    virtual QStringList cameraIdUrlParams() const override;

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

private:
    static CameraDiagnostics::Result checkCameraAvailability(
        const nx::mediaserver::resource::CameraPtr& cameraRes);

    static CameraDiagnostics::Result tryAcquireCameraMediaStream(
        const QnVideoCameraPtr& videoCamera);

    static CameraDiagnostics::Result checkCameraMediaStreamForErrors(
        const nx::mediaserver::resource::CameraPtr& camera);
};
