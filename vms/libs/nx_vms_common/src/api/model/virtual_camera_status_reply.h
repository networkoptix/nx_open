// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

struct QnVirtualCameraStatusReply
{
    /**%apidoc Whether API command was successible. */
    bool success = false;

    /**%apidoc Whether lock was acquired. */
    bool locked = false;

    /**%apidoc Wether this virtual camera is currently consuming virtual footage. */
    bool consuming = false;

    /**%apidoc User who has the lock, if any. */
    QnUuid userId;

    /**%apidoc Lock token if any. */
    QnUuid token;

    /**%apidoc Consume progress, if ongoing. */
    int progress = 0;
};
#define QnVirtualCameraStatusReply_Fields (success)(locked)(consuming)(userId)(token)(progress)

QN_FUSION_DECLARE_FUNCTIONS(QnVirtualCameraStatusReply, (json)(ubjson)(metatype), NX_VMS_COMMON_API)

