/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef CAMERA_DIAGNOSTICS_HANDLER_H
#define CAMERA_DIAGNOSTICS_HANDLER_H

#include <core/resource/resource_fwd.h>
#include <utils/camera/camera_diagnostics.h>

#include <rest/server/json_rest_handler.h>


class QnVideoCamera;

class QnCameraDiagnosticsHandler
:
    public QnJsonRestHandler
{
public:
    QnCameraDiagnosticsHandler();

protected:
    virtual int executeGet(const QString &path, const QnRequestParamList &params, JsonResult &result) override;
    virtual QString description() const override;

private:
    CameraDiagnostics::Result checkCameraAvailability( const QnSecurityCamResourcePtr& cameraRes );
    CameraDiagnostics::Result tryAcquireCameraMediaStream( const QnSecurityCamResourcePtr& cameraRes, QnVideoCamera* videoCamera );
    CameraDiagnostics::Result checkCameraMediaStreamForErrors( QnVideoCamera* videoCamera );
};

#endif  //CAMERA_DIAGNOSTICS_HANDLER_H
