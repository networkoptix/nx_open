// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

struct QnVirtualCameraStatusReply
{
    /**%apidoc Whether API command was successive. */
    bool success = false;

    /**%apidoc Whether lock was acquired. */
    bool locked = false;

    /**%apidoc Whether this virtual camera is currently consuming virtual footage. */
    bool consuming = false;

    /**%apidoc User who has the lock, if any. */
    nx::Uuid userId;

    /**%apidoc Lock token if any. */
    nx::Uuid token;

    /**%apidoc Consume progress, if ongoing. */
    int progress = 0;
};
#define QnVirtualCameraStatusReply_Fields (success)(locked)(consuming)(userId)(token)(progress)

QN_FUSION_DECLARE_FUNCTIONS(QnVirtualCameraStatusReply, (json)(ubjson), NX_VMS_COMMON_API)
