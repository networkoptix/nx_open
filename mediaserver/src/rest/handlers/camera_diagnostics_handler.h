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

// TODO: #Elric rename RestHandler

class QnCameraDiagnosticsHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    QnCameraDiagnosticsHandler();

protected:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) override;
    virtual QString description() const override;

private:
    CameraDiagnostics::Result checkCameraAvailability( const QnSecurityCamResourcePtr& cameraRes );
    CameraDiagnostics::Result tryAcquireCameraMediaStream( const QnSecurityCamResourcePtr& cameraRes, QnVideoCamera* videoCamera );
    CameraDiagnostics::Result checkCameraMediaStreamForErrors( QnResourcePtr res );
};

#endif  //CAMERA_DIAGNOSTICS_HANDLER_H
