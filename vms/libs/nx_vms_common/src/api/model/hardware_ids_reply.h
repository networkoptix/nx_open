// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

struct ApiServerHardwareIdsData
{
    QnUuid serverId;
    QStringList hardwareIds;
};
#define ApiServerHardwareIdsData_Fields (serverId)(hardwareIds)

using ApiServerHardwareIdsDataList = std::vector<ApiServerHardwareIdsData>;

QN_FUSION_DECLARE_FUNCTIONS(ApiServerHardwareIdsData,
    (json)(ubjson)(xml)(csv_record)(metatype),
    NX_VMS_COMMON_API)
