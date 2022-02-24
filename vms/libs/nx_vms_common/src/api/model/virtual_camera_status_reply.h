// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

struct QnVirtualCameraStatusReply
{
    bool success = false;
    bool locked = false;
    bool consuming = false;
    QnUuid userId;
    QnUuid token;
    int progress = 0;
};
#define QnVirtualCameraStatusReply_Fields (success)(locked)(consuming)(userId)(token)(progress)

QN_FUSION_DECLARE_FUNCTIONS(QnVirtualCameraStatusReply, (json)(ubjson)(metatype), NX_VMS_COMMON_API)

