/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef QN_CAMERA_DIAGNOSTICS_REST_HANDLER_H
#define QN_CAMERA_DIAGNOSTICS_REST_HANDLER_H

#include <core/resource/resource_fwd.h>
#include <utils/camera/camera_diagnostics.h>

#include <rest/server/json_rest_handler.h>


class QnVideoCamera;

class QnCameraDiagnosticsRestHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) override;
private:
    CameraDiagnostics::Result checkCameraAvailability( const QnSecurityCamResourcePtr& cameraRes );
    CameraDiagnostics::Result tryAcquireCameraMediaStream( const QnSecurityCamResourcePtr& cameraRes, QnVideoCamera* videoCamera );
    CameraDiagnostics::Result checkCameraMediaStreamForErrors( const QnResourcePtr& res );
};

#endif  //QN_CAMERA_DIAGNOSTICS_REST_HANDLER_H
