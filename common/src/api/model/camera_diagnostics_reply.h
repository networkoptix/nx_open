#ifndef CAMERA_DIAGNOSTICS_REPLY_H
#define CAMERA_DIAGNOSTICS_REPLY_H

#include <QtCore/QMetaType>

#include <utils/camera/camera_diagnostics.h>
#include <utils/common/json.h>


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

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnCameraDiagnosticsReply, (performedStep)(errorCode)(errorParams), inline)

Q_DECLARE_METATYPE(QnCameraDiagnosticsReply)

#endif // QN_TIME_REPLY_H
