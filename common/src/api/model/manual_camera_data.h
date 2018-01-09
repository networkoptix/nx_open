#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include "utils/common/request_param.h"

struct ManualCameraData
{
    QString url;
    QString uniqueId;
    QString manufacturer; // TODO: #wearable this is actually resource type name
};

typedef QList<ManualCameraData> ManualCameraDataList;

#define ManualCameraData_Fields (url)(uniqueId)(manufacturer)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ManualCameraData), (json))

struct AddManualCameraData
{
    ManualCameraDataList cameras;
    QString user;
    QString password;
};

#define AddManualCameraData_Fields (cameras)(user)(password)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((AddManualCameraData), (json))
