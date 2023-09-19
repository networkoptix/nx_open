// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

struct QnVirtualCameraReply
{
    QnUuid id;
};
#define QnVirtualCameraReply_Fields (id)

QN_FUSION_DECLARE_FUNCTIONS(QnVirtualCameraReply, (json)(ubjson), NX_VMS_COMMON_API)
