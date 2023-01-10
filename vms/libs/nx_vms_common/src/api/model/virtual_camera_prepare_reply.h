// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>

struct QnVirtualCameraPrepareReplyElement
{
    QnTimePeriod period;
};
#define QnVirtualCameraPrepareReplyElement_Fields (period)

struct QnVirtualCameraPrepareReply
{
    QVector<QnVirtualCameraPrepareReplyElement> elements;
    bool storageCleanupNeeded = false;
    bool storageFull = false;
};
#define QnVirtualCameraPrepareReply_Fields (elements)(storageCleanupNeeded)(storageFull)

QN_FUSION_DECLARE_FUNCTIONS(QnVirtualCameraPrepareReplyElement, (json)(ubjson))
QN_FUSION_DECLARE_FUNCTIONS(QnVirtualCameraPrepareReply, (json)(ubjson), NX_VMS_COMMON_API)
