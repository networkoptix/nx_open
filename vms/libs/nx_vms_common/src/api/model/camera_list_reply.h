// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

#include <nx/fusion/model_functions_fwd.h>


//!mediaserver's response to \a ping request
class QnCameraListReply
{
public:
    QVector<QString> physicalIdList;
};

#define QnCameraListReply_Fields (physicalIdList)

QN_FUSION_DECLARE_FUNCTIONS(QnCameraListReply, (json)(ubjson), NX_VMS_COMMON_API)
