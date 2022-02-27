// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>

struct ManualCameraData
{
    QString url;
    QString uniqueId;
    QString manufacturer; // TODO: #virtualCamera This is actually resource type name.
};

typedef QList<ManualCameraData> ManualCameraDataList;

#define ManualCameraData_Fields (url)(uniqueId)(manufacturer)

QN_FUSION_DECLARE_FUNCTIONS(ManualCameraData, (json), NX_VMS_COMMON_API)

struct AddManualCameraData
{
    ManualCameraDataList cameras;
    QString user;
    QString password;
};

#define AddManualCameraData_Fields (cameras)(user)(password)

QN_FUSION_DECLARE_FUNCTIONS(AddManualCameraData, (json), NX_VMS_COMMON_API)
