#ifndef CAMERA_DIAGNOSTICS_REPLY_H
#define CAMERA_DIAGNOSTICS_REPLY_H

#include <QtCore/QMetaType>

#include <utils/camera/camera_diagnostics.h>
#include <utils/common/model_functions_fwd.h>


struct QnCameraDiagnosticsReply
{
    //!Constant from \a CameraDiagnostics::Step::Value enumeration
    int performedStep;
    //!Constant from CameraDiagnostics::ErrorCode namespace
    int errorCode;
    //!Parameters of error (number and format is specific for error)
    QList<QString> errorParams;

    QnCameraDiagnosticsReply()
    :
        performedStep( CameraDiagnostics::Step::none ),
        errorCode( CameraDiagnostics::ErrorCode::noError )
    {
    }
};

#define QnCameraDiagnosticsReply_Fields (performedStep)(errorCode)(errorParams)

QN_FUSION_DECLARE_FUNCTIONS(QnCameraDiagnosticsReply, (json)(metatype))

#endif // QN_TIME_REPLY_H
