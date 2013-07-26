/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef CAMERA_DIAGNOSTICS_HANDLER_H
#define CAMERA_DIAGNOSTICS_HANDLER_H

#include <core/resource/resource_fwd.h>
#include <utils/camera/camera_diagnostics.h>

#include "rest/server/request_handler.h"


class QnVideoCamera;

class QnCameraDiagnosticsHandler
:
    public QnRestRequestHandler
{
public:
    QnCameraDiagnosticsHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description() const override;

private:
    CameraDiagnostics::ErrorCode::Value checkCameraAvailability(
        const QnSecurityCamResourcePtr& cameraRes,
        QList<QString>* const errorParams );
    CameraDiagnostics::ErrorCode::Value tryAcquireCameraMediaStream(
        const QnSecurityCamResourcePtr& cameraRes,
        QnVideoCamera* videoCamera,
        QList<QString>* const errorParams );
    CameraDiagnostics::ErrorCode::Value checkCameraMediaStreamForErrors(
        QnVideoCamera* videoCamera,
        QList<QString>* const errorParams );
};

#endif  //CAMERA_DIAGNOSTICS_HANDLER_H
