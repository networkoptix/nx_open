// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utils/camera/camera_diagnostics.h>
#include <nx/fusion/model_functions_fwd.h>

struct NX_VMS_COMMON_API QnCameraDiagnosticsReply
{
    /** Constant from enum CameraDiagnostics::Step::Value. */
    int performedStep;

    /** Constant from namespace enum CameraDiagnostics::ErrorCode. */
    int errorCode;

    /**%apidoc Parameters of the error (number and format is specific for the error). */
    QList<QString> errorParams;

    QnCameraDiagnosticsReply():
        performedStep(CameraDiagnostics::Step::none),
        errorCode(CameraDiagnostics::ErrorCode::noError)
    {
    }
};
#define QnCameraDiagnosticsReply_Fields (performedStep)(errorCode)(errorParams)
QN_FUSION_DECLARE_FUNCTIONS(QnCameraDiagnosticsReply, (json)(ubjson), NX_VMS_COMMON_API)
